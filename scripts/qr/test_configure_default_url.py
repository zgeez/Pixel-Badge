#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Checks for local QR selection, byte preservation and rejected configuration."""

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import configure_default_url as config


class DefaultUrlTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.env = Path(self.directory.name) / ".env"
        self.values = {
            "PORTFOLIO": "https://EXAMPLE.COM",
            "LINKEDIN": "http://EXAMPLE.COM/L",
            "GITHUB": "http://EXAMPLE.COM/G",
            "RESUME": "http://EXAMPLE.COM/R",
        }
        self.write_env()

    def write_env(self, extra=""):
        self.env.write_text("\n".join(
            f'BADGE_URL_{key}="{value}"' for key, value in self.values.items()
        ) + "\n" + extra, encoding="utf-8")

    def test_each_selection_and_scheme_only_normalization(self):
        for key, value in self.values.items():
            scheme, tail = value.split(":", 1)
            self.assertEqual(config.select_url(self.env, key), scheme.upper() + ":" + tail)
        self.assertEqual(config.normalize_url("https://a.b/R", "URL"), "HTTPS://a.b/R")
        self.assertEqual(config.normalize_url("http://a.b/r", "URL"), "HTTP://a.b/r")

    def test_no_execution_or_unrelated_configuration(self):
        self.write_env('TOKEN="PRIVATE SENTINEL"\nNAME=$(irrelevant-command)\n')
        self.assertEqual(len(config.read_links(self.env)), 4)
        self.values["RESUME"] = "${BADGE_URL_PORTFOLIO}"
        self.write_env()
        with self.assertRaisesRegex(ValueError, "BADGE_URL_RESUME"):
            config.read_links(self.env)

    def test_capacity_boundary_and_invalid_syntax(self):
        self.assertEqual(config.normalize_url("http://EXAMPLE.COM/R", "URL"),
                         "HTTP://EXAMPLE.COM/R")
        self.assertEqual(config.normalize_url("https://a.b/ab", "URL"), "HTTPS://a.b/ab")
        for value in ("http://EXAMPLE.COM/RR", "https://a.b/abcd",
                      "HTTPS://", "HTTPS:///R", "HTTP://A/%ZZ", "HTTP://A B",
                      "HTTP://A/\x00", "HTTP://A/é", "HTTP://A/\\R",
                      "HTTP://X@A", "ftp://A.B", 'HTTP://A/"'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                config.normalize_url(value, "URL")

    def test_all_entries_validate_even_if_not_selected(self):
        self.values["RESUME"] = "https://EXAMPLE.COM/too-long"
        self.write_env()
        with self.assertRaisesRegex(ValueError, "BADGE_URL_RESUME"):
            config.select_url(self.env, "PORTFOLIO")

    def test_duplicate_missing_and_unmatched_quote(self):
        self.write_env("BADGE_URL_RESUME=http://A.B/R\n")
        with self.assertRaisesRegex(ValueError, "duplicate"):
            config.read_links(self.env)
        del self.values["RESUME"]
        self.write_env()
        with self.assertRaisesRegex(ValueError, "BADGE_URL_RESUME: missing"):
            config.read_links(self.env)
        self.write_env('BADGE_URL_RESUME="http://A.B/R\n')
        with self.assertRaisesRegex(ValueError, "unmatched quote"):
            config.read_links(self.env)

    def test_representative_build_with_missing_or_bad_env(self):
        self.env.unlink()
        self.assertEqual(config.select_url(self.env, "PORTFOLIO"), config.REPRESENTATIVE)
        with self.assertRaises(ValueError):
            config.select_url(self.env, "LINKEDIN")
        self.env.write_text("BADGE_URL_RESUME=invalid")
        self.assertEqual(config.select_url(self.env, "REPRESENTATIVE"),
                         config.REPRESENTATIVE)

    def test_export_and_bom(self):
        self.env.write_text("\ufeff" + "\n".join(
            f"export BADGE_URL_{key}='{value}'" for key, value in self.values.items()
        ), encoding="utf-8")
        self.assertEqual(config.read_links(self.env)["BADGE_URL_GITHUB"],
                         "HTTP://EXAMPLE.COM/G")

    def test_failed_configuration_keeps_header_and_redacts_values(self):
        header = Path(self.directory.name) / "default_url.hpp"
        header.write_text("previous valid header")
        self.values["RESUME"] = "PRIVATE SENTINEL"
        self.write_env('TOKEN="ANOTHER PRIVATE SENTINEL"\n')
        output = io.StringIO()
        with patch.object(sys, "argv", ["configure", "--env-file", str(self.env),
                                       "--out-header", str(header)]), \
                contextlib.redirect_stderr(output):
            self.assertEqual(config.main(), 1)
        self.assertEqual(header.read_text(), "previous valid header")
        self.assertIn("BADGE_URL_RESUME", output.getvalue())
        self.assertNotIn("PRIVATE SENTINEL", output.getvalue())


if __name__ == "__main__":
    unittest.main()
