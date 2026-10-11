#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate a local prototype default from the four BADGE_URL_* .env keys.

No dotenv expansion or execution is performed. Only the URI scheme is
uppercased; the authority, path, query and fragment retain their exact bytes.
Personal values appear only in the generated build header, never in output.
"""

import argparse
import re
import sys
from pathlib import Path

LINKS = ("PORTFOLIO", "LINKEDIN", "GITHUB", "RESUME")
REPRESENTATIVE = "HTTPS://EXAMPLE.COM"
ALPHANUMERIC = frozenset("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:")
URI_CHARS = frozenset(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-"
    "._~:/?#[]!$&'()*+,;=%"
)


def normalize_url(value: str, key: str) -> str:
    scheme = re.match(r"https?://", value, re.IGNORECASE)
    if scheme is None:
        raise ValueError(f"{key}: expected an HTTP(S) URL")
    value = value[:scheme.end()].upper() + value[scheme.end():]
    remainder = value[scheme.end():]
    if not remainder or remainder[0] in "/?#:":
        raise ValueError(f"{key}: missing URL authority")
    if any(c not in URI_CHARS for c in value):
        raise ValueError(f"{key}: unsupported URL character")
    if re.search(r"%(?![0-9A-Fa-f]{2})", value):
        raise ValueError(f"{key}: invalid percent escape")
    # Nayuki encodeText chooses numeric, alphanumeric or byte encoding for
    # the entire payload. These are its V1-M limits for HTTP(S) URLs.
    limit = 20 if all(c in ALPHANUMERIC for c in value) else 14
    if len(value) > limit:
        raise ValueError(f"{key}: URL does not fit Version 1-M")
    return value


def read_links(path: Path) -> dict[str, str]:
    links = {}
    with path.open(encoding="utf-8-sig") as stream:
        for line in stream:
            match = re.match(
                r"^\s*(?:export\s+)?(BADGE_URL_(?:PORTFOLIO|LINKEDIN|GITHUB|RESUME))"
                r"\s*=\s*(.*?)\s*$", line
            )
            if match is None:
                continue
            key, value = match.groups()
            if key in links:
                raise ValueError(f"{key}: duplicate entry")
            if value.startswith(('"', "'")):
                if len(value) < 2 or value[-1] != value[0]:
                    raise ValueError(f"{key}: unmatched quote")
                value = value[1:-1]
            links[key] = normalize_url(value, key)
    for link in LINKS:
        key = f"BADGE_URL_{link}"
        if key not in links:
            raise ValueError(f"{key}: missing entry")
    return links


def select_url(path: Path, link: str) -> str:
    if link == "REPRESENTATIVE":
        return REPRESENTATIVE
    if not path.exists():
        if link == "PORTFOLIO":
            return REPRESENTATIVE
        raise ValueError("Personal link selection requires an existing .env file")
    return read_links(path)[f"BADGE_URL_{link}"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--env-file", type=Path, required=True)
    parser.add_argument("--link", type=str.upper, choices=(*LINKS, "REPRESENTATIVE"),
                        default="PORTFOLIO")
    parser.add_argument("--out-header", type=Path, required=True)
    args = parser.parse_args()
    try:
        url = select_url(args.env_file, args.link)
        # The validated charset contains neither double quotes nor backslashes.
        header = ('/* SPDX-License-Identifier: MIT */\n'
                  '/* Local build configuration; do not commit. */\n'
                  '#pragma once\n'
                  f'#define BADGE_DEFAULT_URL "{url}"\n')
        args.out_header.parent.mkdir(parents=True, exist_ok=True)
        if not args.out_header.exists() or args.out_header.read_text() != header:
            args.out_header.write_text(header, encoding="utf-8", newline="\n")
    except (ValueError, OSError, UnicodeError) as error:
        # Parsing errors name keys, never values or unrelated .env lines.
        print(f"QR configuration failed: {error}", file=sys.stderr)
        return 1
    print(f"QR default configured: {args.link} (Version 1-M)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
