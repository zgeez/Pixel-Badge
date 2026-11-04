# SPDX-License-Identifier: MIT
import unittest
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


if __name__ == "__main__":
    unittest.main()
