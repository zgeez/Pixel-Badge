#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run KiCad schematic checks and exports for the Rev A hardware project.

- ERC on the full hierarchy; errors and warnings both fail. Violations that
  have been reviewed are excluded in KiCad (with a comment) and are not counted.
- Exports the schematic PDF and a CSV BOM to build/hardware/ (generated
  artifacts; never committed).
- --self-test injects a dangling wire into a temporary copy of the project and
  requires ERC to fail, proving the gate is not a no-op.

kicad-cli is found via the KICAD_CLI environment variable, then PATH, then the
default Windows install location. The KiCad version must match REQUIRED_VERSION
unless --allow-other-version is given.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path

REQUIRED_VERSION = "10.0.6"
REPO_ROOT = Path(__file__).resolve().parents[2]
HW_DIR = REPO_ROOT / "hardware"
ROOT_SCHEMATIC = "smart-led-badge.kicad_sch"
OUT_DIR = REPO_ROOT / "build" / "hardware"
WINDOWS_DEFAULT = Path(r"C:\Program Files\KiCad\10.0\bin\kicad-cli.exe")


def find_kicad_cli() -> str:
    candidates = [os.environ.get("KICAD_CLI"), shutil.which("kicad-cli")]
    if os.name == "nt":
        candidates.append(str(WINDOWS_DEFAULT))
    for c in candidates:
        if c and Path(c).exists():
            return c
    sys.exit("error: kicad-cli not found (set KICAD_CLI or add it to PATH)")


def run(cli: str, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run([cli, *args], capture_output=True, text=True)


def check_version(cli: str, allow_other: bool) -> None:
    version = run(cli, "version").stdout.strip()
    print(f"kicad-cli {version} ({cli})")
    if version != REQUIRED_VERSION and not allow_other:
        sys.exit(f"error: KiCad {REQUIRED_VERSION} required, found {version} "
                 "(use --allow-other-version to override)")


def erc(cli: str, schematic: Path, report: Path) -> int:
    """Run ERC; return the number of error/warning violations (non-excluded)."""
    report.parent.mkdir(parents=True, exist_ok=True)
    result = run(cli, "sch", "erc", "--severity-error", "--severity-warning",
                 "--format", "json", "--output", str(report), str(schematic))
    if result.returncode != 0 or not report.exists():
        print(result.stdout, result.stderr, file=sys.stderr)
        sys.exit("error: kicad-cli ERC did not run")
    data = json.loads(report.read_text(encoding="utf-8"))
    violations = [(sheet["path"], v) for sheet in data.get("sheets", [])
                  for v in sheet.get("violations", [])]
    for path, v in violations:
        print(f"  {v['severity'].upper():7} {path} {v['type']}: {v['description']}")
    return len(violations)


def export(cli: str, schematic: Path) -> None:
    for args, name in ((("pdf",), "schematic.pdf"), (("bom",), "bom.csv")):
        out = OUT_DIR / name
        result = run(cli, "sch", "export", *args, "--output", str(out), str(schematic))
        if result.returncode != 0:
            print(result.stdout, result.stderr, file=sys.stderr)
            sys.exit(f"error: export {name} failed")
        print(f"exported {out.relative_to(REPO_ROOT)}")


def self_test(cli: str) -> int:
    with tempfile.TemporaryDirectory() as tmp:
        copy = Path(tmp) / "hardware"
        shutil.copytree(HW_DIR, copy)
        root = copy / ROOT_SCHEMATIC
        text = root.read_text(encoding="utf-8")
        wire = ("\t(wire\n\t\t(pts\n\t\t\t(xy 50.8 50.8) (xy 76.2 50.8)\n\t\t)\n"
                "\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n"
                f'\t\t(uuid "{uuid.uuid4()}")\n\t)\n')
        marker = "\t(sheet_instances"
        if marker not in text:
            sys.exit("error: self-test could not find the injection point")
        root.write_text(text.replace(marker, wire + marker, 1), encoding="utf-8", newline="\n")
        count = erc(cli, root, Path(tmp) / "erc.json")
    if count == 0:
        print("SELF-TEST FAILED: injected dangling wire was not reported", file=sys.stderr)
        return 1
    print(f"self-test passed: injected fault reported ({count} violations)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--self-test", action="store_true",
                        help="verify that ERC fails on an injected violation")
    parser.add_argument("--erc-only", action="store_true", help="skip PDF/BOM export")
    parser.add_argument("--allow-other-version", action="store_true")
    args = parser.parse_args()

    cli = find_kicad_cli()
    check_version(cli, args.allow_other_version)
    if args.self_test:
        return self_test(cli)

    schematic = HW_DIR / ROOT_SCHEMATIC
    count = erc(cli, schematic, OUT_DIR / "erc.json")
    if count:
        print(f"ERC failed: {count} violation(s); report: build/hardware/erc.json",
              file=sys.stderr)
        return 1
    print("ERC passed (0 errors, 0 warnings)")
    if not args.erc_only:
        export(cli, schematic)
    return 0


if __name__ == "__main__":
    sys.exit(main())
