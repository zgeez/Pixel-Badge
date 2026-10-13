#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check that app build defines contain only validated public contact links."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class ConfigureLinksTest(unittest.TestCase):
    def run_generator(self, text):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            env_file = root / '.env'
            output = root / 'defines.json'
            if text is not None:
                env_file.write_text(text, encoding='utf-8')
            result = subprocess.run([
                sys.executable, str(Path(__file__).with_name('configure_links.py')),
                '--env-file', str(env_file), '--out', str(output),
            ], capture_output=True, text=True, check=False)
            contents = output.read_text() if output.exists() else None
            return result, contents

    def test_whitelist_and_preserve_path_case(self):
        keys = ['PORTFOLIO', 'LINKEDIN', 'GITHUB', 'RESUME']
        inputs = '\n'.join(f'BADGE_URL_{key}=https://A.B/{index}'
                           for index, key in enumerate(keys))
        result, contents = self.run_generator(
            inputs + '\nSECRET_TOKEN=DO_NOT_EMIT\nBADGE_URL_OTHER=DO_NOT_EMIT\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(contents), {
            f'BADGE_URL_{key}': f'HTTPS://A.B/{index}'
            for index, key in enumerate(keys)
        })
        self.assertNotIn('DO_NOT_EMIT', contents + result.stdout + result.stderr)
        self.assertNotIn('HTTPS://A.B', result.stdout)
        result, contents = self.run_generator(
            '\n'.join(f'BADGE_URL_{key}=https://A.B/x' for key in keys))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(all(value == 'HTTPS://A.B/x'
                            for value in json.loads(contents).values()))

    def test_missing_file_produces_empty_contact_defines(self):
        result, contents = self.run_generator(None)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(contents), {
            f'BADGE_URL_{key}': ''
            for key in ['PORTFOLIO', 'LINKEDIN', 'GITHUB', 'RESUME']
        })

    def test_incomplete_or_invalid_links_do_not_produce_defines(self):
        for text in ['BADGE_URL_PORTFOLIO=https://A.B',
                     'BADGE_URL_PORTFOLIO=DO_NOT_EMIT']:
            result, contents = self.run_generator(text)
            self.assertNotEqual(result.returncode, 0)
            self.assertIsNone(contents)
            self.assertNotIn('DO_NOT_EMIT', result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
