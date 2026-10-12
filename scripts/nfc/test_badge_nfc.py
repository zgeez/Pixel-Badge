# SPDX-License-Identifier: MIT
import contextlib
import io
import json
import struct
import subprocess
import threading
from types import SimpleNamespace
from unittest.mock import Mock, patch
import unittest
import urllib.error
import urllib.request
from badge_nfc import Mailbox, PhoneProof, NfcOperationError, normalize_url, wait_rf_proof, provision


class Link:
    def __init__(self):
        self.memory = bytearray(120)
        struct.pack_into("<4I", self.memory, 0, 0x424E4643, 2, 0, 0)
        self.reads = []

    def memory_read8(self, address, count):
        self.reads.append((address, count))
        return self.memory[address:address + count]

    def memory_write8(self, address, values):
        self.memory[address:address + len(values)] = bytes(values)

    def memory_write32(self, address, values):
        struct.pack_into("<I", self.memory, address, values[0])
        if address == 8:
            struct.pack_into("<I", self.memory, 12, values[0])

    def memory_read32(self, address, count):
        return struct.unpack_from("<I", self.memory, address)


class Tests(unittest.TestCase):
    def test_only_scheme_is_normalized(self):
        self.assertEqual(normalize_url("hTtPs://ZIADG.COM/L"), "HTTPS://ZIADG.COM/L")
        self.assertEqual(normalize_url("http://A/%2f?x=Y"), "HTTP://A/%2f?x=Y")
        self.assertEqual(normalize_url("invalid"), "invalid")

    def test_mailbox_never_reads_credentials_and_clears_after_command(self):
        link = Link()
        mailbox = Mailbox(link, 0)
        mailbox.command(3, credentials=b"\1\2\3\4\5\6")
        self.assertEqual(link.memory[112:], bytes(8))
        self.assertTrue(all(address + size <= 88 for address, size in link.reads))

    def test_busy_mailbox_rejected(self):
        link = Link()
        struct.pack_into("<I", link.memory, 8, 1)
        with self.assertRaises(RuntimeError):
            Mailbox(link, 0)

    def test_legacy_firmware_mailbox_rejected(self):
        link = Link()
        struct.pack_into("<I", link.memory, 4, 1)
        with self.assertRaises(RuntimeError):
            Mailbox(link, 0)

    def test_failed_command_clears_credentials(self):
        link = Link()
        struct.pack_into("<i", link.memory, 20, -5)
        with self.assertRaisesRegex(NfcOperationError, r"NFC command 3 failed \(-5\)"):
            Mailbox(link, 0).command(3, credentials=b"\1\2\3\4\5\6")
        self.assertEqual(link.memory[112:], bytes(8))

    def test_rf_timeout_identifies_unstarted_and_incomplete_checks(self):
        proof = SimpleNamespace(complete=threading.Event(), consumed=False, accepted=False)
        with self.assertRaisesRegex(NfcOperationError, "never fetched the challenge"):
            wait_rf_proof(proof, timeout=0)
        proof.consumed = True
        with self.assertRaisesRegex(NfcOperationError, "sent no proof"):
            wait_rf_proof(proof, timeout=0)

    def test_rf_confirmation_requires_accepted_proof(self):
        proof = SimpleNamespace(complete=threading.Event(), consumed=True, accepted=False)
        proof.complete.set()
        with self.assertRaisesRegex(NfcOperationError, "final confirmation was not sent"):
            wait_rf_proof(proof, timeout=0)
        proof.accepted = True
        wait_rf_proof(proof, timeout=0)

    def test_missing_proof_never_prompts_or_confirms_and_cleanup_preserves_error(self):
        proof = SimpleNamespace(complete=threading.Event(), consumed=False, accepted=False,
                                port=5052, token="synthetic-session")
        mailbox = Mock()
        mailbox.command.return_value = {"uid": bytes(7)}
        args = SimpleNamespace(port=5052, android_serial="fictional-phone", adb="adb")

        def fake_adb(_args, _serial, *command):
            if "--remove" in command:
                raise subprocess.CalledProcessError(1, "adb", stderr=b"must not print native payload")

        output = io.StringIO()
        with patch("badge_nfc.PhoneProof") as factory, patch("badge_nfc.adb", side_effect=fake_adb), \
                patch("badge_nfc.secrets.token_bytes", return_value=bytes(range(6))), \
                patch("badge_nfc.wait_rf_proof", side_effect=lambda p: wait_rf_proof(p, timeout=0)), \
                patch("builtins.input") as enter, contextlib.redirect_stdout(output):
            factory.return_value.__enter__.return_value = proof
            with self.assertRaisesRegex(NfcOperationError, "helper never fetched the challenge"):
                provision(args, mailbox)
            enter.assert_not_called()
        mailbox.command.assert_called_once()
        self.assertEqual(mailbox.command.call_args.args[0], 3)
        self.assertEqual(mailbox.command.call_args.kwargs["credentials"], bytes(6))
        self.assertNotIn("native payload", output.getvalue())

    def test_loopback_requires_nonce_two_proofs_and_uid(self):
        output = io.StringIO()
        proof = PhoneProof(bytes(7), bytes([1, 2, 3, 4, 5, 6]), 0)
        with contextlib.redirect_stderr(output), contextlib.redirect_stdout(output), proof:
            base = f"http://127.0.0.1:{proof.port}"
            with self.assertRaises(urllib.error.HTTPError):
                urllib.request.urlopen(base + "/challenge")
            headers = {"Authorization": "Bearer " + proof.token}
            request = urllib.request.Request(base + "/challenge", headers=headers)
            with urllib.request.urlopen(request) as response:
                challenge = json.load(response)
            self.assertEqual(challenge["uid"], bytes(7).hex())
            with self.assertRaises(urllib.error.HTTPError):
                urllib.request.urlopen(request)
            result = {"uid": bytes(7).hex(), "generated_ok": True, "factory_rejected": True,
                      "fresh_sessions": True, "proof_version": 2}
            request = urllib.request.Request(base + "/proof", data=json.dumps(result).encode(), headers=headers)
            with urllib.request.urlopen(request) as response:
                self.assertTrue(json.load(response)["accepted"])
            self.assertTrue(proof.complete.is_set())
            with self.assertRaises(urllib.error.HTTPError):
                urllib.request.urlopen(request)
        self.assertEqual(proof.credentials, bytes(6))
        self.assertEqual(output.getvalue(), "")

    def test_incomplete_or_wrong_uid_proof_rejected(self):
        for uid, fresh in [("ff" * 7, True), ("00" * 7, False)]:
            with PhoneProof(bytes(7), bytes(6), 0) as proof:
                base = f"http://127.0.0.1:{proof.port}"
                headers = {"Authorization": "Bearer " + proof.token}
                with urllib.request.urlopen(urllib.request.Request(base + "/challenge", headers=headers)):
                    pass
                result = {"uid": uid, "generated_ok": True, "factory_rejected": True,
                          "fresh_sessions": fresh, "proof_version": 2}
                request = urllib.request.Request(base + "/proof", data=json.dumps(result).encode(), headers=headers)
                with urllib.request.urlopen(request) as response:
                    self.assertFalse(json.load(response)["accepted"])

    def test_legacy_or_missing_proof_version_rejected(self):
        for version in [None, 1, True, 2.0, "2"]:
            with PhoneProof(bytes(7), bytes(6), 0) as proof:
                base = f"http://127.0.0.1:{proof.port}"
                headers = {"Authorization": "Bearer " + proof.token}
                with urllib.request.urlopen(urllib.request.Request(base + "/challenge", headers=headers)):
                    pass
                result = {"uid": bytes(7).hex(), "generated_ok": True, "factory_rejected": True,
                          "fresh_sessions": True}
                if version is not None:
                    result["proof_version"] = version
                request = urllib.request.Request(base + "/proof", data=json.dumps(result).encode(), headers=headers)
                with urllib.request.urlopen(request) as response:
                    self.assertFalse(json.load(response)["accepted"])


if __name__ == "__main__":
    unittest.main()
