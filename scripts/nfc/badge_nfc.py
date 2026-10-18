#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Owner-triggered NFC debug commands. Credentials exist in RAM only."""
import argparse
import hmac
import json
import secrets
import struct
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

MAGIC = 0x424E4643
PROOF_VERSION = 3
MAILBOX_VERSION = 4
PACKAGE = "dev.badge.nfc"


class NfcOperationError(RuntimeError):
    """Only fixed diagnostic text and numeric errno; never native payloads."""


def wipe(value):
    value[:] = b"\0" * len(value)


class Mailbox:
    def __init__(self, link, address):
        self.link, self.address = link, address
        header = bytes(link.memory_read8(address, 16))
        magic, version, request, response = struct.unpack("<4I", header)
        if magic != MAGIC or version != MAILBOX_VERSION or request != response:
            raise NfcOperationError("NFC mailbox unavailable or busy")
        self.sequence = request

    def snapshot(self):
        # Deliberately excludes command input and the credential region.
        data = bytes(self.link.memory_read8(self.address, 96))
        words = struct.unpack("<8I", data[:32])
        return {
            "result": struct.unpack("<i", data[20:24])[0],
            "ready": words[6] == 1,
            "address": words[7],
            "uid": data[32:39],
            "desired": data[40:68].split(b"\0", 1)[0].decode("ascii"),
            "verified": data[68:96].split(b"\0", 1)[0].decode("ascii"),
        }

    def command(self, command, payload=b"", credentials=b"", timeout=12):
        completed = False
        try:
            if len(payload) > 25 or len(credentials) > 6:
                raise ValueError("NFC command input too long")
            self.link.memory_write8(self.address + 96, list(payload.ljust(28, b"\0")))
            self.link.memory_write8(self.address + 124, list(credentials.ljust(8, b"\0")))
            self.link.memory_write32(self.address + 16, [command])
            self.sequence = (self.sequence + 1) & 0xFFFFFFFF
            self.link.memory_write32(self.address + 8, [self.sequence])
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                response = self.link.memory_read32(self.address + 12, 1)[0]
                if response == self.sequence:
                    result = self.snapshot()
                    if result["result"] != 0:
                        raise NfcOperationError(f"NFC command {command} failed ({result['result']})")
                    completed = True
                    return result
                time.sleep(0.02)
            raise NfcOperationError(f"NFC command {command} timed out; inspect non-secret RTT status")
        finally:
            try:
                self.link.memory_write8(self.address + 124, [0] * 8)
            except Exception:
                # A lost SWD link may prevent erasure. Preserve an existing
                # failure; never report success if cleanup itself failed.
                if completed:
                    raise NfcOperationError("NFC credential cleanup failed") from None


class PhoneProof:
    """Single-use loopback challenge; no HTTP access logs or credential files."""

    def __init__(self, uid, credentials, port=5052):
        self.uid = bytes(uid)
        self.credentials = bytearray(credentials)
        self.token = secrets.token_hex(32)
        self.complete = threading.Event()
        self.accepted = False
        self.consumed = False
        owner = self

        class Handler(BaseHTTPRequestHandler):
            def setup(self):
                self.request.settimeout(5)
                super().setup()

            def log_message(self, *_):
                pass

            def send(self, status, payload):
                data = json.dumps(payload).encode()
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def authorized(self):
                auth = self.headers.get("Authorization", "")
                return hmac.compare_digest(auth, "Bearer " + owner.token)

            def do_GET(self):
                if self.path != "/challenge" or not self.authorized() or owner.consumed:
                    self.send(403, {})
                    return
                owner.consumed = True
                self.send(200, {
                    "uid": owner.uid.hex(),
                    "pwd": owner.credentials[:4].hex(),
                    "pack": owner.credentials[4:6].hex(),
                    "proof_version": PROOF_VERSION,
                })

            def do_POST(self):
                if self.path != "/proof" or not self.authorized() or owner.complete.is_set():
                    self.send(403, {})
                    return
                try:
                    length = int(self.headers.get("Content-Length", "0"))
                    if length < 1 or length > 512:
                        raise ValueError()
                    result = json.loads(self.rfile.read(length))
                    owner.accepted = (
                        owner.consumed
                        and result.get("uid") == owner.uid.hex()
                        and result.get("generated_ok") is True
                        and result.get("repeated_generated_ok") is True
                        and result.get("fresh_sessions") is True
                        and type(result.get("proof_version")) is int
                        and result.get("proof_version") == PROOF_VERSION
                    )
                except (ValueError, TypeError):
                    owner.accepted = False
                owner.complete.set()
                self.send(200, {"accepted": owner.accepted})

        class QuietServer(HTTPServer):
            def handle_error(self, request, client_address):
                pass

        self.server = QuietServer(("127.0.0.1", port), Handler)
        self.port = self.server.server_port
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *_):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        wipe(self.credentials)
        self.token = ""


def elf_mailbox(path):
    from elftools.elf.elffile import ELFFile
    with Path(path).open("rb") as stream:
        symbols = ELFFile(stream).get_section_by_name(".symtab")
        found = symbols.get_symbol_by_name("badge_nfc_mailbox") if symbols else None
        if not found or found[0]["st_size"] != 132:
            raise NfcOperationError("Build the NFC provisioning configuration first")
        return found[0]["st_value"]


def adb(args, serial, *command):
    subprocess.run([args.adb, "-s", serial, *command], check=True, capture_output=True)


def wait_rf_proof(proof, timeout=100):
    if not proof.complete.wait(timeout):
        stage = ("helper fetched the challenge but sent no proof" if proof.consumed
                 else "helper never fetched the challenge")
        raise NfcOperationError(f"RF verification timed out: {stage}; final confirmation was not sent")
    if not proof.accepted:
        raise NfcOperationError("RF proof rejected; final confirmation was not sent")


def configure(mailbox):
    """Explicit wired setup; no phone verifier and no RF-authentication claim."""
    credentials = bytearray(secrets.token_bytes(6))
    try:
        while credentials[:4] == b"\xff" * 4:
            wipe(credentials)
            credentials[:] = secrets.token_bytes(6)
        result = mailbox.command(3, credentials=credentials)
        mailbox.command(6, payload=result["uid"])
        print("NFC configured; local policy and URL checks passed. No RF authentication test performed.", flush=True)
    finally:
        wipe(credentials)


def provision(args, mailbox):
    credentials = bytearray(secrets.token_bytes(6))
    while credentials[:4] == b"\xff" * 4:
        wipe(credentials)
        credentials[:] = secrets.token_bytes(6)
    try:
        result = mailbox.command(3, credentials=credentials)
        with PhoneProof(result["uid"], credentials, args.port) as proof:
            adb(args, args.android_serial, "reverse", f"tcp:{proof.port}", f"tcp:{proof.port}")
            try:
                adb(args, args.android_serial, "shell", "am", "force-stop", PACKAGE)
                adb(args, args.android_serial, "shell", "am", "start", "-n",
                    PACKAGE + "/.ProvisionActivity", "--es", "session", proof.token,
                    "--ei", "port", str(proof.port))
                print("On Android: follow both authentication checks in the NFC helper, lifting the phone fully away between them.", flush=True)
                print("Waiting up to 100 seconds for helper RF proof. Enter is used only after the next prompt.", flush=True)
                wait_rf_proof(proof)
                print("RF proof accepted. Remove the phone and any other reader from the tag, then press Enter to finish the guarded I2C check.", flush=True)
                input()
                print("Sending final confirmation to the DK...", flush=True)
                mailbox.command(4, payload=result["uid"])
                print("NFC provisioning verified; no irreversible configuration locks set.", flush=True)
            finally:
                try:
                    adb(args, args.android_serial, "reverse", "--remove", f"tcp:{proof.port}")
                except (subprocess.CalledProcessError, OSError):
                    # Cleanup must not hide the original RF/firmware diagnostic.
                    print("ADB reverse cleanup failed; reconnect the phone before retrying.", flush=True)
    finally:
        wipe(credentials)


def normalize_url(url):
    # Match the .env configuration policy: normalize only the scheme so short
    # links can fit V1 alphanumeric mode; preserve paths and escapes exactly.
    for scheme in ("https://", "http://"):
        if url[:len(scheme)].lower() == scheme:
            return scheme.upper() + url[len(scheme):]
    return url


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["status", "sync", "set-url", "configure", "provision"])
    parser.add_argument("--elf", default="build/nfc-provisioning/nrf5340_prototype/zephyr/zephyr.elf")
    parser.add_argument("--jlink-serial", type=int)
    parser.add_argument("--url")
    parser.add_argument("--android-serial")
    parser.add_argument("--adb", default="adb")
    parser.add_argument("--port", type=int, default=5052)
    args = parser.parse_args()
    if args.action == "provision" and not args.android_serial:
        parser.error("provision requires --android-serial; no unverified fallback is available")
    if args.action == "set-url" and not args.url:
        parser.error("set-url requires --url")
    import pylink
    try:
        with pylink.JLink(log=None, detailed_log=None, error=None, warn=None) as link:
            link.open(serial_no=args.jlink_serial)
            link.set_tif(pylink.enums.JLinkInterfaces.SWD)
            link.connect("NRF5340_XXAA_APP", speed=1000)
            mailbox = Mailbox(link, elf_mailbox(args.elf))
            if args.action == "configure":
                configure(mailbox)
            elif args.action == "provision":
                provision(args, mailbox)
            elif args.action == "status":
                state = mailbox.snapshot()
                print(json.dumps({k: v for k, v in state.items() if k != "uid"}))
            else:
                mailbox.command(2 if args.action == "set-url" else 5,
                                payload=normalize_url(args.url).encode("ascii") if args.url else b"")
                print("QR and NFC URL verified.")
    except NfcOperationError as error:
        # Only our sanitized fixed/numeric diagnostics may reach the console.
        print(f"NFC operation failed: {error}. Keep the tag out of service.")
        return 1
    except (Exception, KeyboardInterrupt):
        # Never print native exception objects or request bodies: provisioning
        # credentials must not reach console logs or traceback dumps.
        print("NFC operation failed. Keep the tag out of service and inspect non-secret RTT status.")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
