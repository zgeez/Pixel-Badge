#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Fail if private, personal, or generated files are tracked or addable.

Checks every file Git tracks or would add (untracked and not ignored), so it
catches mistakes both locally before a commit and in CI.
"""

import fnmatch
import subprocess
import sys
from pathlib import Path

ALLOWED = {".env.example"}

FORBIDDEN = {
    ".env": "personal configuration (commit .env.example instead)",
    ".env.*": "personal configuration (commit .env.example instead)",
    ".private/*": "local-only notes",
    "AGENTS.md": "local-only notes",
    "CLAUDE.md": "local-only notes",
    "build/*": "build output",
    "*/build/*": "build output",
    "twister-out*/*": "test output",
    ".history/*": "KiCad local history",
    "*/.history/*": "KiCad local history",
    "*.gbr": "fabrication output (release artifact)",
    "*.gbrjob": "fabrication output (release artifact)",
    "*.drl": "fabrication output (release artifact)",
    "*.hex": "firmware image (release artifact)",
    "*.elf": "firmware image (release artifact)",
}


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=root,
        check=True,
        capture_output=True,
    )
    paths = [p for p in result.stdout.decode().split("\0") if p]

    violations = []
    for path in paths:
        if path in ALLOWED:
            continue
        for pattern, reason in FORBIDDEN.items():
            if fnmatch.fnmatchcase(path, pattern):
                violations.append(f"{path}: {reason}")
                break

    if violations:
        print("Repository hygiene check failed:", file=sys.stderr)
        for v in violations:
            print(f"  {v}", file=sys.stderr)
        return 1

    print(f"Repository hygiene check passed ({len(paths)} files).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
