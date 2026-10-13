#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Emit only the four public contact-link defines from local dotenv input."""
import argparse
import importlib.util
import json
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--env-file', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / 'qr' / 'configure_default_url.py'
    spec = importlib.util.spec_from_file_location('badge_qr_links', source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    try:
        links = module.read_links(args.env_file)
    except FileNotFoundError:
        links = {f'BADGE_URL_{key}': '' for key in module.LINKS}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(links, indent=2) + '\n', encoding='utf-8')
    print('Configured four contact links; values omitted.')


if __name__ == '__main__':
    main()
