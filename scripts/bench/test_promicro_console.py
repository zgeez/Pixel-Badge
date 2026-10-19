# SPDX-License-Identifier: MIT
import unittest
from promicro_console import public_status


class StatusTests(unittest.TestCase):
    def test_public_runtime_status_and_missing_hardware(self):
        status = public_status("BENCH uptime=42375 load=0 frames=2037 frame_crc=7ad0e605 mode=0 brightness=128 NFC=-19")
        self.assertEqual(status["frame_crc"], 0x7ad0e605)
        self.assertEqual(status["nfc_result"], -19)
        self.assertEqual(status["load"], 0)

    def test_passkeys_and_other_usb_lines_are_never_public_status(self):
        for line in ("Pairing code: 123456", "No current pairing code", "BENCH load=0", ""):
            self.assertIsNone(public_status(line))
        status = public_status("BENCH uptime=1 load=0 frames=1 frame_crc=00000000 mode=0 brightness=13 code=123456")
        self.assertNotIn("code", status)
        self.assertNotIn("123456", str(status))


if __name__ == "__main__":
    unittest.main()
