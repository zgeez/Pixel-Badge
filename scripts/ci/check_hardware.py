#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run KiCad schematic checks and exports for the Rev A hardware project.

- ERC on the full hierarchy; errors and warnings both fail. Violations that
  have been reviewed are excluded in KiCad (with a comment) and are not counted.
- Pin/pad check: every placed symbol's pin numbers must equal its footprint's
  numbered copper pads (unnumbered pads are mechanical tabs or paste
  apertures). It compares pad-number sets only: it catches a footprint with
  missing, extra or differently numbered pads, not one with the same numbers
  and different geometry (for example SOT-353 for SOT-23-5), and it does not
  prove pin functions match the datasheet. Both are reviewed per part.
- Exports the schematic PDF and a CSV BOM to build/hardware/ (generated
  artifacts; never committed).
- --self-test injects a dangling wire into a temporary copy of the project and
  requires a violation that refers to that wire's UUID (and more violations
  than a baseline run), so it still proves the gate works when the schematic
  has unrelated violations. It then gives one part a footprint with too few
  pads and requires the pin/pad check to name that part.

kicad-cli is found via the KICAD_CLI environment variable, then PATH, then the
default Windows install location. The KiCad version must match REQUIRED_VERSION
unless --allow-other-version is given.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import uuid
import xml.etree.ElementTree as ET
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


def erc(cli: str, schematic: Path, report: Path, quiet: bool = False) -> list:
    """Run ERC; return the error/warning violations (excluded ones are omitted)."""
    report.parent.mkdir(parents=True, exist_ok=True)
    result = run(cli, "sch", "erc", "--severity-error", "--severity-warning",
                 "--format", "json", "--output", str(report), str(schematic))
    if result.returncode != 0 or not report.exists():
        print(result.stdout, result.stderr, file=sys.stderr)
        sys.exit("error: kicad-cli ERC did not run")
    data = json.loads(report.read_text(encoding="utf-8"))
    violations = [(sheet["path"], v) for sheet in data.get("sheets", [])
                  for v in sheet.get("violations", [])]
    if not quiet:
        for path, v in violations:
            print(f"  {v['severity'].upper():7} {path} {v['type']}: {v['description']}")
    return [v for _, v in violations]


def export(cli: str, schematic: Path) -> None:
    for args, name in ((("pdf",), "schematic.pdf"), (("bom",), "bom.csv")):
        out = OUT_DIR / name
        result = run(cli, "sch", "export", *args, "--output", str(out), str(schematic))
        if result.returncode != 0:
            print(result.stdout, result.stderr, file=sys.stderr)
            sys.exit(f"error: export {name} failed")
        print(f"exported {out.relative_to(REPO_ROOT)}")


def footprint_dirs(cli: str, hw_dir: Path) -> dict:
    """Map footprint library nicknames to directories: the project table first,
    then KiCad's standard libraries next to kicad-cli (or KICAD10_FOOTPRINT_DIR)."""
    libs = {}
    table = (hw_dir / "fp-lib-table").read_text(encoding="utf-8")
    # One chunk per (lib ...) entry; S-expression whitespace and line breaks
    # are free, and simple tokens may be unquoted.
    for entry in re.split(r"\(lib\s", table)[1:]:
        fields = {k: q or bare for k, q, bare in
                  re.findall(r'\((name|uri)\s+(?:"([^"]*)"|([^\s()"]+))\s*\)', entry)}
        if "name" in fields and "uri" in fields:
            libs[fields["name"]] = Path(fields["uri"].replace("${KIPRJMOD}", str(hw_dir)))
    candidates = [os.environ.get("KICAD10_FOOTPRINT_DIR"),
                  Path(cli).resolve().parent.parent / "share" / "kicad" / "footprints",
                  "/usr/share/kicad/footprints"]
    std = next((Path(c) for c in candidates if c and Path(c).is_dir()), None)
    if std is None:
        sys.exit("error: KiCad footprint libraries not found (set KICAD10_FOOTPRINT_DIR)")
    for d in std.glob("*.pretty"):
        libs.setdefault(d.stem, d)
    return libs


def copper_pads(path: Path) -> set:
    """Numbered pads with copper; unnumbered pads are mechanical or paste-only."""
    text = path.read_text(encoding="utf-8")
    pads = set()
    for num, kind, layers in re.findall(r'\(pad\s+"([^"]*)"\s+(\w+).*?\(layers\s+([^)]*)\)',
                                        text, re.S):
        if num and kind != "np_thru_hole" and "Cu" in layers:
            pads.add(num)
    return pads


def pin_pad_check(cli: str, schematic: Path, work: Path) -> list:
    """Return one message per placed part whose pins and footprint pads differ."""
    netlist = work / "netlist.xml"
    result = run(cli, "sch", "export", "netlist", "--format", "kicadxml",
                 "--output", str(netlist), str(schematic))
    if result.returncode != 0 or not netlist.exists():
        print(result.stdout, result.stderr, file=sys.stderr)
        sys.exit("error: netlist export failed")
    root = ET.parse(netlist).getroot()
    pins = {(lp.get("lib"), lp.get("part")): {p.get("num") for p in lp.iter("pin")}
            for lp in root.iter("libpart")}
    libs = footprint_dirs(cli, schematic.parent)
    cache, problems = {}, []
    for comp in root.iter("comp"):
        ref, fp = comp.get("ref"), (comp.findtext("footprint") or "").strip()
        if not fp:
            continue  # parts without a footprint (for example the PCB coil) are not placed
        src = comp.find("libsource")
        lib, name = fp.split(":", 1)
        path = libs.get(lib, Path()) / f"{name}.kicad_mod"
        if not path.is_file():
            problems.append(f"{ref}: footprint {fp} not found")
            continue
        if fp not in cache:
            cache[fp] = copper_pads(path)
        sym = pins.get((src.get("lib"), src.get("part")), set())
        missing, extra = sorted(sym - cache[fp]), sorted(cache[fp] - sym)
        if missing or extra:
            problems.append(f"{ref} ({fp}): pins without pad {missing}, pads without pin {extra}")
    return problems


# Violation types KiCad reports for a wire connected to nothing.
INJECTED_TYPES = {"wire_dangling", "unconnected_wire_endpoint"}
# Part given a wrong footprint by the pin/pad self-test (U707, SN74LVC1G04).
SELF_TEST_SHEET = "matrix_rows.kicad_sch"
SELF_TEST_LIB_ID = "74xGxx:74LVC1G04"
SELF_TEST_FOOTPRINT = "Package_TO_SOT_SMD:SOT-23-5"
SELF_TEST_REF = "U707"


def self_test(cli: str) -> int:
    """Inject a dangling wire into a copy of the project and require ERC to
    report a violation that refers to that specific wire."""
    with tempfile.TemporaryDirectory() as tmp:
        copy = Path(tmp) / "hardware"
        shutil.copytree(HW_DIR, copy)
        root = copy / ROOT_SCHEMATIC
        text = root.read_text(encoding="utf-8")
        baseline = erc(cli, root, Path(tmp) / "baseline.json", quiet=True)

        wire_uuid = str(uuid.uuid4())
        wire = ("\t(wire\n\t\t(pts\n\t\t\t(xy 50.8 50.8) (xy 76.2 50.8)\n\t\t)\n"
                "\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n"
                f'\t\t(uuid "{wire_uuid}")\n\t)\n')
        marker = "\t(sheet_instances"
        if marker not in text:
            sys.exit("error: self-test could not find the injection point")
        root.write_text(text.replace(marker, wire + marker, 1), encoding="utf-8", newline="\n")
        injected = erc(cli, root, Path(tmp) / "erc.json", quiet=True)

        # Wrong footprint: the 5-pin inverter on a 3-pad SOT-23.
        sheet = copy / "sheets" / SELF_TEST_SHEET
        text = sheet.read_text(encoding="utf-8")
        start = text.find(f'(lib_id "{SELF_TEST_LIB_ID}")')
        prop = f'(property "Footprint" "{SELF_TEST_FOOTPRINT}"'
        at = text.find(prop, start)
        if start < 0 or at < 0:
            sys.exit("error: self-test could not find the footprint to replace")
        text = text[:at] + '(property "Footprint" "Package_TO_SOT_SMD:SOT-23"' + text[at + len(prop):]
        sheet.write_text(text, encoding="utf-8", newline="\n")
        pad_problems = pin_pad_check(cli, root, Path(tmp))

    matches = [v for v in injected if v["type"] in INJECTED_TYPES and
               any(item.get("uuid") == wire_uuid for item in v.get("items", []))]
    if not matches:
        print("SELF-TEST FAILED: no violation refers to the injected wire "
              f"({len(baseline)} baseline, {len(injected)} after injection)", file=sys.stderr)
        return 1
    if len(injected) <= len(baseline):
        print("SELF-TEST FAILED: violation count did not increase after injection",
              file=sys.stderr)
        return 1
    print(f"self-test passed: injected wire reported ({len(matches)} matching "
          f"violation(s); {len(baseline)} baseline, {len(injected)} after injection)")
    if not any(p.startswith(f"{SELF_TEST_REF} ") for p in pad_problems):
        print(f"SELF-TEST FAILED: pin/pad check did not report {SELF_TEST_REF} "
              f"on a 3-pad footprint ({pad_problems})", file=sys.stderr)
        return 1
    print(f"self-test passed: pin/pad check reported {SELF_TEST_REF} on a wrong footprint")
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
    count = len(erc(cli, schematic, OUT_DIR / "erc.json"))
    if count:
        print(f"ERC failed: {count} violation(s); report: build/hardware/erc.json",
              file=sys.stderr)
        return 1
    print("ERC passed (0 errors, 0 warnings)")
    problems = pin_pad_check(cli, schematic, OUT_DIR)
    for p in problems:
        print(f"  {p}")
    if problems:
        print(f"Pin/pad check failed: {len(problems)} part(s)", file=sys.stderr)
        return 1
    print("Pin/pad check passed (symbol pin numbers = footprint pad numbers for every placed part)")
    if not args.erc_only:
        export(cli, schematic)
    return 0


if __name__ == "__main__":
    sys.exit(main())
