# SPDX-License-Identifier: MIT
"""Measure the matrix scan timing in a sigrok session (.sr) of the DK bench test.

Channels: D0 SCK, D1 MOSI, D2 LAT, D3 BLANK (high = rows off), D4 frame marker.
Prints distributions (value in microseconds x count) and the checks used in the
result record. Needs numpy; the nRF Connect SDK toolchain's Python has it.

    python analyze.py halt-capture.sr
"""

import configparser
import re
import sys
import zipfile
from collections import Counter

import numpy as np

SCK, MOSI, LAT, BLANK, FRAME = range(5)
ROWS = 32


def load(path):
    with zipfile.ZipFile(path) as z:
        meta = configparser.ConfigParser()
        meta.read_string(z.read("metadata").decode())
        rate = meta["device 1"]["samplerate"].split()
        fs = float(rate[0]) * {"Hz": 1, "kHz": 1e3, "MHz": 1e6}[rate[1]]
        prefix = meta["device 1"]["capturefile"] + "-"
        chunks = sorted((n for n in z.namelist() if n.startswith(prefix)),
                        key=lambda n: int(re.search(r"(\d+)$", n).group(1)))
        data = np.concatenate([np.frombuffer(z.read(n), dtype=np.uint8) for n in chunks])
    return fs, data


def edges(data, bit):
    level = (data >> bit) & 1
    idx = np.flatnonzero(np.diff(level)) + 1
    return level, idx[level[idx] == 1], idx[level[idx] == 0]


def high_widths(rise, fall):
    """Start and width of each high interval that is complete in the capture."""
    fall = fall[np.searchsorted(fall, rise[0]):] if rise.size else fall
    n = min(rise.size, fall.size)
    return rise[:n], fall[:n] - rise[:n]


def show(name, samples, us, decimals=1):
    counts = Counter(float(v) for v in np.round(samples / us, decimals))
    items = ", ".join(f"{k:g}x{v}" for k, v in sorted(counts.items()))
    print(f"{name} (us): n={samples.size}: {items}")


def main():
    fs, d = load(sys.argv[1])
    us = fs / 1e6
    print(f"{d.size} samples at {fs / 1e6:g} MS/s = {d.size / fs:.3f} s")

    _, lat_r, lat_f = edges(d, LAT)
    blank, bl_r, bl_f = edges(d, BLANK)
    _, sck_r, _ = edges(d, SCK)
    _, fr_r, fr_f = edges(d, FRAME)
    mosi = (d >> MOSI) & 1

    halt = np.argmax(np.diff(lat_r))
    halt_start, halt_end = lat_r[halt], lat_r[halt + 1]
    # Intervals during normal scanning; the halt is reported separately below.
    def running(x, limit_us=1000):
        return x[x < limit_us * us]

    show("LAT rise to rise", running(np.diff(lat_r)), us)
    show("LAT high", high_widths(lat_r, lat_f)[1], us, 2)
    _, widths = high_widths(bl_r, bl_f)
    show("BLANK high", running(widths), us)
    show("BLANK low (row on)", high_widths(bl_f, bl_r)[1], us)
    show("BLANK rise to LAT rise", running(lat_r[1:] - bl_r[np.searchsorted(bl_r, lat_r[1:]) - 1]), us)
    show("LAT rise to BLANK fall", bl_f[np.searchsorted(bl_f, lat_r[:-1])] - lat_r[:-1], us)

    in_blank = blank[lat_r].all() and blank[lat_f - 1].all()
    print(f"every LAT pulse inside BLANK high: {bool(in_blank)}")

    bursts = np.split(sck_r, np.flatnonzero(np.diff(sck_r) > 2 * us) + 1)
    print("SCK rising edges per shift:", dict(Counter(b.size for b in bursts)))
    first = np.array([b[0] for b in bursts])
    last = np.array([b[-1] for b in bursts])
    show("first to last SCK rise", last - first, us)
    prev_fall = np.searchsorted(lat_f, first) - 1
    show("LAT fall to first SCK rise", first[prev_fall >= 0] - lat_f[prev_fall[prev_fall >= 0]], us, 2)
    nxt = np.searchsorted(lat_r, last)
    ok = nxt < lat_r.size
    show("last SCK rise to next LAT rise", running(lat_r[nxt[ok]] - last[ok]), us, 0)

    rows, errors = {}, 0
    for b in bursts:
        if b.size != 64:
            errors += 1
            continue
        by = np.packbits(mosi[b]).tolist()
        sel = ~int.from_bytes(bytes(by[:4]), "big") & 0xFFFFFFFF
        row = sel.bit_length() - 1
        if sel == 0 or sel & (sel - 1) or by[4:] != [0xA5, row, 0x5A, ~row & 0xFF]:
            errors += 1
            continue
        rows[b[0]] = row
    seq = list(rows.values())
    breaks = sum(1 for a, b in zip(seq, seq[1:]) if b != (a + 1) % ROWS)
    print(f"decoded shifts {len(rows)}, pattern errors {errors}, row order breaks {breaks}")

    _, fwidth = high_widths(fr_r, fr_f)
    show("frame marker high", fwidth, us, 2)
    show("frame marker period", running(np.diff(fr_r), 100000), us, 0)
    after = np.searchsorted(first, fr_r)
    print("row in the first shift of each frame:",
          dict(Counter(rows.get(first[i]) for i in after if i < first.size)))
    show("last BLANK rise to frame marker rise", running(fr_r - bl_r[np.searchsorted(bl_r, fr_r) - 1]), us, 2)
    nl = np.searchsorted(lat_r, fr_f)
    show("frame marker fall to next LAT rise", lat_r[nl[nl < lat_r.size]] - fr_f[nl < lat_r.size], us, 2)

    last_frame = fr_r[fr_r < halt_start][-1]
    lats = int(np.sum((lat_r > last_frame) & (lat_r <= halt_start)))
    off = bl_r[bl_r < halt_end][-1]
    quiet = d[off + 1: halt_end - int(5 * us)]
    print(f"halt: LAT stops at {halt_start / fs:.4f} s, resumes at {halt_end / fs:.4f} s "
          f"({(halt_end - halt_start) / fs:.4f} s); LAT pulses in the last frame {lats}/{ROWS}")
    print(f"halt: BLANK high from {off / fs:.6f} s for {(halt_end - off) / fs:.4f} s; levels "
          + ", ".join(f"D{k} {int(((quiet >> k) & 1).min())}..{int(((quiet >> k) & 1).max())}"
                      for k in range(5)))


if __name__ == "__main__":
    main()
