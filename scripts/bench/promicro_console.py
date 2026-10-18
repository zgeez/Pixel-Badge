# SPDX-License-Identifier: MIT
"""Interactive USB display for the headless Pro Micro bench.

Pairing codes appear only in the interactive terminal, never in the optional
public-status JSON. Requires pyserial (included in the pinned Nordic SDK).
"""
import argparse
import json
import re
import sys
import time
from pathlib import Path


def public_status(line):
    if not line.startswith("BENCH "):
        return None
    fields = dict(re.findall(r"([a-z_]+)=(-?[0-9a-f]+)", line))
    required = ("uptime", "load", "frames", "frame_crc", "mode", "brightness")
    if any(k not in fields for k in required):
        return None
    try:
        result = {k: int(fields[k]) for k in required if k != "frame_crc"}
        result["frame_crc"] = int(fields["frame_crc"], 16)
        nfc = re.search(r"NFC=(-?\d+)", line)
        if nfc:
            result["nfc_result"] = int(nfc[1])
        return result
    except ValueError:
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--pair", action="store_true")
    parser.add_argument("--status-file", type=Path)
    args = parser.parse_args()
    if not sys.stdout.isatty():
        parser.error("Use an interactive terminal; pairing codes must not be redirected to logs")
    import msvcrt
    import serial
    print("Pixel Badge Bench | p: pair again | s: status | q: quit")
    print("Open Pixel Badge and select Pixel Badge Bench when pairing is ready.")
    with serial.Serial(args.port, 115200, timeout=0.05) as port:
        port.dtr = True
        time.sleep(0.2)
        port.write(b"p" if args.pair else b"h")
        pending = bytearray()
        next_poll = 0.0
        displayed = False
        while True:
            if msvcrt.kbhit():
                key = msvcrt.getwch().lower()
                if key == "q":
                    break
                if key in ("p", "s", "h"):
                    port.write(key.encode("ascii"))
            now = time.monotonic()
            if now >= next_poll:
                port.write(b"ks")
                next_poll = now + 1.0
            pending.extend(port.read(256))
            if len(pending) > 2048:
                pending.clear()
            while b"\n" in pending:
                raw, _, rest = pending.partition(b"\n")
                pending[:] = rest
                line = raw.decode("ascii", errors="replace").strip()
                code = re.fullmatch(r"Pairing code: (\d{6})", line)
                if code:
                    print("\rEnter on phone: " + code[1] + " " * 35, end="", flush=True)
                    displayed = True
                    continue
                if line.startswith("No current pairing code"):
                    if displayed:
                        print("\rCode cleared. Check the phone for the pairing result." + " " * 10)
                        displayed = False
                    continue
                status = public_status(line)
                if status is not None:
                    if args.status_file:
                        args.status_file.parent.mkdir(parents=True, exist_ok=True)
                        temporary = args.status_file.with_suffix(".tmp")
                        temporary.write_text(json.dumps(status), encoding="utf-8")
                        temporary.replace(args.status_file)
                    continue
                if line:
                    print("\r" + line + " " * 10)
        print("\r" + " " * 72 + "\rUSB display closed.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
