# SPDX-License-Identifier: MIT
"""Read or clear the Rev A bench battery log over J-Link SWD.

The log lives in the flash reserve partition (0xf2000, 24 KiB; see
firmware/rev_a_nrf52840/pm_static.yml) and is written by firmware built with
configs/battery-log.conf. Records are 16 bytes (badge/battery_log.hpp).
Run in the NCS Python environment, which provides pylink-square.

  python scripts/bench/battery_log.py read --csv build/battery-log.csv
  python scripts/bench/battery_log.py clear

clear refuses a chip whose UICR records a bootloader (a Pro Micro bench).
"""
import argparse
import csv
import struct
import sys
from pathlib import Path

LOG_ADDRESS = 0xF2000
LOG_SIZE = 0x6000
PAGE_SIZE = 0x1000
RECORD = struct.Struct("<IHHhBBhH")
FLAGS = {
    "loaded": 1,
    "usb": 2,
    "charging": 4,
    "shed": 8,
    "low_power": 16,
    "start": 32,
    "end": 64,
    "no_current": 128,
}
# nRF52840 NVMC (Product Specification, NVMC chapter).
NVMC_READY = 0x4001E400
NVMC_CONFIG = 0x4001E504
NVMC_ERASEPAGE = 0x4001E508
# UICR.NRFFW[0]: start of a bootloader the MBR starts (nRF52840 PS, UICR
# chapter). Erased (0xffffffff) on a Rev A badge; 0xf4000 on a Pro Micro
# with the UF2 bootloader, which the log pages would overlap.
UICR_NRFFW0 = 0x10001014
NO_BOOTLOADER = 0xFFFFFFFF


def decode(blob):
    """Records up to the first erased slot, split into sessions at start flags."""
    sessions = []
    for offset in range(0, len(blob) - RECORD.size + 1, RECORD.size):
        chunk = blob[offset : offset + RECORD.size]
        if chunk == b"\xff" * RECORD.size:
            break
        seconds, saadc, charger, ibat, ceiling, flags, tdie, ts = RECORD.unpack(chunk)
        row = {
            "seconds": seconds,
            "saadc_mv": saadc,
            "charger_mv": charger,
            "ibat_ma": ibat,
            "ceiling": ceiling,
            "tdie_c": tdie / 10,
            "ts_permille": ts,
        }
        row.update({name: int(flags & bit != 0) for name, bit in FLAGS.items()})
        if row["start"] or not sessions:
            sessions.append([])
        sessions[-1].append(row)
    return sessions


def add_capacity(session):
    """Cumulative mAh drawn from the pack (positive), from the period means."""
    used = 0.0
    previous = None
    for row in session:
        if previous is not None and not row["no_current"]:
            used -= row["ibat_ma"] * (row["seconds"] - previous) / 3600
        row["used_mah"] = round(used, 1)
        row["hours"] = round(row["seconds"] / 3600, 3)
        previous = row["seconds"]


def summary(index, session):
    first, last = session[0], session[-1]
    end = "end record" if last["end"] else "no end record (still running or power lost)"
    return (
        f"session {index}: {len(session)} records, {last['hours']:.2f} h, "
        f"SAADC {first['saadc_mv']} -> {last['saadc_mv']} mV, "
        f"charger {first['charger_mv']} -> {last['charger_mv']} mV, "
        f"{last['used_mah']:.0f} mAh drawn, {end}"
    )


def connect():
    import pylink

    link = pylink.JLink(log=None, detailed_log=None, error=None, warn=None)
    link.open()
    link.set_tif(pylink.enums.JLinkInterfaces.SWD)
    link.connect("NRF52840_XXAA", speed=4000)
    return link


def read(args):
    link = connect()
    try:
        blob = bytes(link.memory_read8(LOG_ADDRESS, LOG_SIZE))
    finally:
        link.close()
    sessions = decode(blob)
    if not sessions:
        print("Battery log is empty.")
        return 0
    rows = []
    for index, session in enumerate(sessions, 1):
        add_capacity(session)
        print(summary(index, session))
        rows += [{"session": index, **row} for row in session]
    if args.csv:
        path = Path(args.csv)
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
        print(f"Wrote {len(rows)} records to {path}")
    return 0


def bootloader_refusal(address):
    """Why clear must not erase this chip, or None for a Rev A badge."""
    if address == NO_BOOTLOADER:
        return None
    return (
        f"Refusing to clear: UICR.NRFFW[0] records a bootloader at 0x{address:08x}. "
        "This is a UF2 or DFU board such as the Pro Micro bench, not a Rev A badge, "
        "and erasing the log pages would damage its bootloader. Nothing was erased."
    )


def clear(_args):
    link = connect()
    try:
        refusal = bootloader_refusal(link.memory_read32(UICR_NRFFW0, 1)[0])
        if refusal:
            print(refusal)
            return 1
        link.halt()
        link.memory_write32(NVMC_CONFIG, [2])  # erase enable
        for page in range(LOG_ADDRESS, LOG_ADDRESS + LOG_SIZE, PAGE_SIZE):
            link.memory_write32(NVMC_ERASEPAGE, [page])
            while link.memory_read32(NVMC_READY, 1)[0] & 1 == 0:
                pass
        link.memory_write32(NVMC_CONFIG, [0])  # read only
        blob = bytes(link.memory_read8(LOG_ADDRESS, LOG_SIZE))
        link.reset(halt=False)
    finally:
        link.close()
    if blob != b"\xff" * LOG_SIZE:
        print("Battery log clear could not be verified.")
        return 1
    print("Battery log cleared (24 KiB reserve erased and verified); badge reset.")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="action", required=True)
    read_parser = sub.add_parser("read", help="decode the log, optionally to CSV")
    read_parser.add_argument("--csv", help="write every record to this CSV file")
    sub.add_parser("clear", help="erase the log pages and reset the badge")
    args = parser.parse_args()
    return read(args) if args.action == "read" else clear(args)


if __name__ == "__main__":
    sys.exit(main())
