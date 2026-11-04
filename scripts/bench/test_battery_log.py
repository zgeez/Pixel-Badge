# SPDX-License-Identifier: MIT
import unittest
from unittest import mock

import battery_log
from battery_log import RECORD, add_capacity, decode

ERASED = b"\xff" * RECORD.size


def record(seconds, ibat, flags, saadc=3900):
    return RECORD.pack(seconds, saadc, saadc + 5, ibat, 255, flags, 250, 520)


class DecodeTests(unittest.TestCase):
    def test_sessions_split_at_start_and_stop_at_erased(self):
        blob = (
            record(0, 0, 32 | 128)
            + record(120, -150, 1)
            + record(240, -150, 1 | 64)
            + record(0, 0, 32 | 128)
            + ERASED
            + record(9, 9, 1)
        )
        sessions = decode(blob)
        self.assertEqual([len(s) for s in sessions], [3, 1])
        self.assertEqual(sessions[0][-1]["end"], 1)
        self.assertEqual(sessions[0][1]["ibat_ma"], -150)
        self.assertEqual(sessions[0][0]["tdie_c"], 25.0)

    def test_capacity_integrates_period_means(self):
        session = decode(record(0, 0, 32 | 128) + record(1800, -200, 1) + record(3600, -100, 1))[0]
        add_capacity(session)
        self.assertEqual([r["used_mah"] for r in session], [0.0, 100.0, 150.0])
        self.assertEqual(session[-1]["hours"], 1.0)

    def test_empty_log(self):
        self.assertEqual(decode(ERASED * 4), [])


class FakeLink:
    """J-Link stand-in: flash reads erased, NVMC always ready."""

    def __init__(self, nrffw0):
        self.nrffw0 = nrffw0
        self.erased_pages = []
        self.halted = False

    def memory_read32(self, address, count):
        if address == battery_log.UICR_NRFFW0:
            return [self.nrffw0]
        return [1] * count

    def memory_write32(self, address, values):
        if address == battery_log.NVMC_ERASEPAGE:
            self.erased_pages += values

    def memory_read8(self, address, count):
        return [0xFF] * count

    def halt(self):
        self.halted = True

    def reset(self, halt):
        pass

    def close(self):
        pass


class ClearTests(unittest.TestCase):
    def run_clear(self, nrffw0):
        link = FakeLink(nrffw0)
        with mock.patch.object(battery_log, "connect", return_value=link), mock.patch(
            "builtins.print"
        ):
            rc = battery_log.clear(None)
        return rc, link

    def test_refuses_a_board_with_a_bootloader(self):
        rc, link = self.run_clear(0xF4000)  # Pro Micro UF2 bootloader
        self.assertEqual(rc, 1)
        self.assertEqual(link.erased_pages, [])
        self.assertFalse(link.halted)

    def test_erases_the_log_pages_on_a_badge(self):
        rc, link = self.run_clear(battery_log.NO_BOOTLOADER)
        self.assertEqual(rc, 0)
        self.assertEqual(link.erased_pages, list(range(0xF2000, 0xF8000, 0x1000)))

    def test_refusal_message_only_for_a_bootloader(self):
        self.assertIsNone(battery_log.bootloader_refusal(0xFFFFFFFF))
        self.assertIn("0x000f4000", battery_log.bootloader_refusal(0xF4000))


if __name__ == "__main__":
    unittest.main()
