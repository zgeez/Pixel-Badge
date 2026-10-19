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
from badge_nfc import Mailbox, PhoneProof, NfcOperationError, normalize_url, wait_rf_proof, provision, configure


class Link:
    def __init__(self):
        self.memory = bytearray(132)
        struct.pack_into("<4I", self.memory, 0, 0x424E4643, 4, 0, 0)
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
    def test_wired_configuration_needs_no_phone_and_wipes_credentials(self):
        for fail_at in (0, 1, 2):
            with self.subTest(fail_at=fail_at):
                seen = []
                calls = []
                def command(number, **kwargs):
                    calls.append((number, kwargs.get("payload")))
                    if "credentials" in kwargs:
                        seen.append(kwargs["credentials"])
                    if len(calls) == fail_at:
                        raise NfcOperationError("test failure")
                    return {"uid": bytes(range(7))}
                mailbox = Mock()
                mailbox.command.side_effect = command
                with patch("badge_nfc.PhoneProof") as phone, \
                        patch("badge_nfc.secrets.token_bytes", return_value=bytes(range(6))), \
                        contextlib.redirect_stdout(io.StringIO()):
                    if fail_at:
                        with self.assertRaises(NfcOperationError):
                            configure(mailbox)
                    else:
                        configure(mailbox)
                    phone.assert_not_called()
                self.assertEqual(seen[0], bytes(6))
                self.assertEqual(calls[0][0], 3)
                if fail_at != 1:
                    self.assertEqual(calls[1], (6, bytes(range(7))))

    def test_only_scheme_is_normalized(self):
        self.assertEqual(normalize_url("hTtPs://ZIADG.COM/L"), "HTTPS://ZIADG.COM/L")
        self.assertEqual(normalize_url("http://A/%2f?x=Y"), "HTTP://A/%2f?x=Y")
        self.assertEqual(normalize_url("invalid"), "invalid")

    def test_mailbox_never_reads_credentials_and_clears_after_command(self):
        link = Link()
        mailbox = Mailbox(link, 0)
        mailbox.command(3, credentials=b"\1\2\3\4\5\6")
        self.assertEqual(link.memory[124:], bytes(8))
        self.assertTrue(all(address + size <= 96 for address, size in link.reads))

    def test_full_length_urls_have_separate_terminated_mailbox_slots(self):
        link = Link()
        value = b"HTTPS://EXAMPLE.COM/ABCDE"
        self.assertEqual(len(value), 25)
        link.memory[40:68] = value.ljust(28, b"\0")
        link.memory[68:96] = value.ljust(28, b"\0")
        result = Mailbox(link, 0).command(2, payload=value)
        self.assertEqual(result["desired"], value.decode())
        self.assertEqual(result["verified"], value.decode())
        self.assertEqual(link.memory[96:124], value.ljust(28, b"\0"))
        self.assertEqual(link.memory[124:], bytes(8))
        self.assertTrue(all(address + size <= 96 for address, size in link.reads))

    def test_busy_mailbox_rejected(self):
        link = Link()
        struct.pack_into("<I", link.memory, 8, 1)
        with self.assertRaises(RuntimeError):
            Mailbox(link, 0)

    def test_legacy_firmware_mailbox_rejected(self):
        for version in [1, 2, 3]:
            link = Link()
            struct.pack_into("<I", link.memory, 4, version)
            with self.assertRaises(RuntimeError):
                Mailbox(link, 0)

    def test_failed_command_clears_credentials(self):
        link = Link()
        struct.pack_into("<i", link.memory, 20, -5)
        with self.assertRaisesRegex(NfcOperationError, r"NFC command 3 failed \(-5\)"):
            Mailbox(link, 0).command(3, credentials=b"\1\2\3\4\5\6")
        self.assertEqual(link.memory[124:], bytes(8))

    def test_setup_write_failures_clear_even_partially_written_credentials(self):
        for fail_at in range(1, 5):
            with self.subTest(write=fail_at):
                link = Link()
                link.memory[124:] = bytes([7] * 8)
                writes = 0

                def fail_write(original):
                    def write(address, values):
                        nonlocal writes
                        writes += 1
                        if writes == fail_at:
                            if address == 124:
                                link.memory[124:127] = bytes(values[:3])
                            raise OSError("synthetic setup failure")
                        return original(address, values)
                    return write

                link.memory_write8 = fail_write(link.memory_write8)
                link.memory_write32 = fail_write(link.memory_write32)
                with self.assertRaisesRegex(OSError, "synthetic setup failure"):
                    Mailbox(link, 0).command(3, credentials=bytes(range(6)))
                self.assertEqual(link.memory[124:], bytes(8))
                self.assertEqual(writes, fail_at + 1)

    def test_cleanup_failure_preserves_original_setup_error(self):
        link = Link()
        link.memory_write8 = Mock(side_effect=[
            OSError("synthetic setup failure"), OSError("synthetic cleanup failure")])
        with self.assertRaisesRegex(OSError, "synthetic setup failure"):
            Mailbox(link, 0).command(3)
        self.assertEqual(link.memory_write8.call_args.args, (124, [0] * 8))

    def test_cleanup_failure_cannot_report_command_success(self):
        link = Link()
        original = link.memory_write8

        def write(address, values):
            if address == 124 and values == [0] * 8:
                raise OSError("must not expose native payload")
            original(address, values)

        link.memory_write8 = write
        with self.assertRaisesRegex(NfcOperationError, "^NFC credential cleanup failed$"):
            Mailbox(link, 0).command(3, credentials=bytes(range(6)))

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
            with self.assertRaises(urllib.error.HTTPError) as denied:
                urllib.request.urlopen(base + "/challenge")
            with denied.exception:
                self.assertEqual(denied.exception.code, 403)
            headers = {"Authorization": "Bearer " + proof.token}
            request = urllib.request.Request(base + "/challenge", headers=headers)
            with urllib.request.urlopen(request) as response:
                challenge = json.load(response)
            self.assertEqual(challenge["uid"], bytes(7).hex())
            self.assertEqual(challenge["proof_version"], 3)
            with self.assertRaises(urllib.error.HTTPError) as denied:
                urllib.request.urlopen(request)
            with denied.exception:
                self.assertEqual(denied.exception.code, 403)
            result = {"uid": bytes(7).hex(), "generated_ok": True, "repeated_generated_ok": True,
                      "fresh_sessions": True, "proof_version": 3}
            request = urllib.request.Request(base + "/proof", data=json.dumps(result).encode(), headers=headers)
            with urllib.request.urlopen(request) as response:
                self.assertTrue(json.load(response)["accepted"])
            self.assertTrue(proof.complete.is_set())
            with self.assertRaises(urllib.error.HTTPError) as denied:
                urllib.request.urlopen(request)
            with denied.exception:
                self.assertEqual(denied.exception.code, 403)
        self.assertEqual(proof.credentials, bytes(6))
        self.assertEqual(output.getvalue(), "")

    def test_incomplete_or_wrong_uid_proof_rejected(self):
        for uid, fresh in [("ff" * 7, True), ("00" * 7, False)]:
            with PhoneProof(bytes(7), bytes(6), 0) as proof:
                base = f"http://127.0.0.1:{proof.port}"
                headers = {"Authorization": "Bearer " + proof.token}
                with urllib.request.urlopen(urllib.request.Request(base + "/challenge", headers=headers)):
                    pass
                result = {"uid": uid, "generated_ok": True, "repeated_generated_ok": True,
                          "fresh_sessions": fresh, "proof_version": 3}
                request = urllib.request.Request(base + "/proof", data=json.dumps(result).encode(), headers=headers)
                with urllib.request.urlopen(request) as response:
                    self.assertFalse(json.load(response)["accepted"])

    def test_legacy_or_missing_proof_version_rejected(self):
        for version in [None, 1, 2, True, 3.0, "3"]:
            with PhoneProof(bytes(7), bytes(6), 0) as proof:
                base = f"http://127.0.0.1:{proof.port}"
                headers = {"Authorization": "Bearer " + proof.token}
                with urllib.request.urlopen(urllib.request.Request(base + "/challenge", headers=headers)):
                    pass
                result = {"uid": bytes(7).hex(), "generated_ok": True, "repeated_generated_ok": True,
                          "fresh_sessions": True}
                if version is not None:
                    result["proof_version"] = version
                request = urllib.request.Request(base + "/proof", data=json.dumps(result).encode(), headers=headers)
                with urllib.request.urlopen(request) as response:
                    self.assertFalse(json.load(response)["accepted"])

    def test_both_positive_checks_must_be_true_without_legacy_substitution(self):
        for first, second in [(False, True), (True, False), (True, None), (1, True), (True, 1)]:
            with self.subTest(first=first, second=second), PhoneProof(bytes(7), bytes(6), 0) as proof:
                base = f"http://127.0.0.1:{proof.port}"
                headers = {"Authorization": "Bearer " + proof.token}
                with urllib.request.urlopen(urllib.request.Request(base + "/challenge", headers=headers)):
                    pass
                result = {"uid": bytes(7).hex(), "generated_ok": first, "factory_rejected": True,
                          "fresh_sessions": True, "proof_version": 3}
                if second is not None:
                    result["repeated_generated_ok"] = second
                request = urllib.request.Request(base + "/proof", data=json.dumps(result).encode(), headers=headers)
                with urllib.request.urlopen(request) as response:
                    self.assertFalse(json.load(response)["accepted"])


if __name__ == "__main__":
    unittest.main()
