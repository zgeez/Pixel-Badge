/* SPDX-License-Identifier: MIT */
import dev.badge.nfc.NfcProof;
import java.io.IOException;
import java.util.Arrays;

public final class NfcProofTest {
    private static void check(boolean condition) { if (!condition) { throw new AssertionError(); } }
    private static byte[] cc() {
        byte[] result = new byte[16];
        result[0] = (byte) 0xe1; result[1] = 0x10; result[2] = 0x6d;
        return result;
    }
    public static void main(String[] args) throws Exception {
        // Fictional credentials only; no physical tag or credential log.
        byte[] pwd = {1, 2, 3, 4}, pack = {5, 6};
        check(NfcProof.supportedTag(command -> {
            check(Arrays.equals(command, new byte[]{0x60}));
            return new byte[]{0, 4, 4, 5, 2, 2, 0x13, 3};
        }));
        check(!NfcProof.supportedTag(command -> new byte[]{0, 4, 4, 5, 2, 2, 0x15, 3}));
        check(NfcProof.generatedPassword(command -> {
            check(Arrays.equals(command, new byte[]{0x1b, 1, 2, 3, 4}));
            return new byte[]{5, 6};
        }, pwd, pack));
        check(!NfcProof.generatedPassword(command -> new byte[]{0, 0}, pwd, pack));
        check(!NfcProof.factoryPasswordRejected(command -> command[0] == 0x30 ? cc() : new byte[]{5, 6}));
        check(NfcProof.factoryPasswordRejected(command -> command[0] == 0x30 ? cc() : new byte[]{0}));
        // Model accepted factory authentication with its PACK lost in transit.
        // Public reads still work; that must never turn the error into proof.
        final int[] exchanges = {0};
        IOException lostReply = new IOException();
        try {
            NfcProof.factoryPasswordRejected(command -> {
                ++exchanges[0];
                if (command[0] == 0x30) { return cc(); }
                throw lostReply;
            });
            throw new AssertionError("Lost authentication reply must be inconclusive");
        } catch (IOException expected) { check(expected == lostReply); }
        check(exchanges[0] == 2); // No healthy read used to reinterpret the error.
        for (byte error : new byte[]{1, 3, 4, 5, 7, 10, 16, -1}) {
            check(!NfcProof.factoryPasswordRejected(command -> command[0] == 0x30 ? cc() : new byte[]{error}));
        }
        for (byte[] malformed : new byte[][]{null, {}, {0, 0}, {0, 0, 0}}) {
            check(!NfcProof.factoryPasswordRejected(command -> command[0] == 0x30 ? cc() : malformed));
        }
        final int[] reads = {0};
        try {
            NfcProof.factoryPasswordRejected(command -> {
                if (command[0] != 0x30) { return new byte[]{0}; }
                if (reads[0]++ == 0) { return cc(); }
                throw new IOException(); // Explicit NAK followed by link loss.
            });
            throw new AssertionError("Link loss must not count as password rejection");
        } catch (IOException expected) {}
        check(!NfcProof.factoryPasswordRejected(command -> new byte[16]));
        final int[] sequence = {0};
        check(NfcProof.verifiedSession(command -> {
            int index = sequence[0]++;
            if (index == 1) {
                check(Arrays.equals(command, new byte[]{0x1b, 1, 2, 3, 4}));
                return new byte[]{5, 6};
            }
            check(Arrays.equals(command, new byte[]{0x30, 0x03}));
            return cc();
        }, pwd, pack));
        check(sequence[0] == 3);
        for (byte[] malformed : new byte[][]{null, {}, {5}, {5, 7}, {5, 6, 0}}) {
            check(!NfcProof.verifiedSession(command -> command[0] == 0x30 ? cc() : malformed, pwd, pack));
        }
        for (int failAt = 0; failAt < 3; ++failAt) {
            final int failedExchange = failAt;
            final int[] count = {0};
            IOException transport = new IOException();
            try {
                NfcProof.verifiedSession(command -> {
                    if (count[0]++ == failedExchange) { throw transport; }
                    return command[0] == 0x30 ? cc() : new byte[]{5, 6};
                }, pwd, pack);
                throw new AssertionError("Every lost reply must fail positive verification");
            } catch (IOException expected) { check(expected == transport); }
            check(count[0] == failAt + 1);
        }
        check(!NfcProof.verifiedSession(command -> new byte[16], pwd, pack));
        final int[] changedCc = {0};
        check(!NfcProof.verifiedSession(command -> {
            if (command[0] == 0x1b) { return new byte[]{5, 6}; }
            return changedCc[0]++ == 0 ? cc() : new byte[16];
        }, pwd, pack));
        check(!NfcProof.verifiedSession(command -> { throw new AssertionError("Factory password must not be tried"); },
                new byte[]{-1, -1, -1, -1}, pack));
        check(!NfcProof.verifiedSession(command -> { throw new AssertionError("Invalid password must not be tried"); },
                new byte[3], pack));
        System.out.println("NFC RF verifier logic: rejection and positive-session checks passed");
    }
}
