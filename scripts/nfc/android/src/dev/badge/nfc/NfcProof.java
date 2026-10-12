/* SPDX-License-Identifier: MIT */
package dev.badge.nfc;

import java.io.IOException;
import java.security.MessageDigest;
import java.util.Arrays;

/** RF command logic shared with the local JVM regression tests. */
public final class NfcProof {
    public static final int VERSION = 2;
    public interface Radio { byte[] exchange(byte[] command) throws IOException; }

    private NfcProof() {}

    public static boolean supportedTag(Radio radio) throws IOException {
        // NTAG I2C plus 1 kB, datasheet Rev.3.6 Table 22.
        byte[] reply = radio.exchange(new byte[]{0x60});
        return Arrays.equals(reply, new byte[]{0, 4, 4, 5, 2, 2, 0x13, 3});
    }

    public static boolean generatedPassword(Radio radio, byte[] pwd, byte[] pack) throws IOException {
        if (pwd.length != 4 || pack.length != 2) { return false; }
        byte[] command = {0x1b, pwd[0], pwd[1], pwd[2], pwd[3]};
        byte[] reply = null;
        try {
            reply = radio.exchange(command);
            return reply != null && reply.length == 2 && MessageDigest.isEqual(reply, pack);
        } finally {
            Arrays.fill(command, (byte) 0);
            if (reply != null) { Arrays.fill(reply, (byte) 0); }
        }
    }

    private static boolean publicRead(Radio radio) throws IOException {
        byte[] reply = radio.exchange(new byte[]{0x30, 0x03});
        return reply != null && reply.length == 16 && (reply[0] & 255) == 0xe1
                && reply[1] == 0x10 && reply[2] == 0x6d && reply[3] == 0;
    }

    public static boolean factoryPasswordRejected(Radio radio) throws IOException {
        if (!publicRead(radio)) { return false; }
        byte[] reply = null;
        try {
            reply = radio.exchange(new byte[]{0x1b, -1, -1, -1, -1});
            // NTAG I2C plus Rev.3.6 Table 17: 0h is wrong password;
            // parity/CRC, arbitration and AUTHLIM errors are not that proof.
            // IOException (including a lost PACK) is inconclusive, even if
            // a subsequent public read could succeed. Propagate it unchanged.
            return reply != null && reply.length == 1 && reply[0] == 0
                    && publicRead(radio);
        } finally {
            if (reply != null) { Arrays.fill(reply, (byte) 0); }
        }
    }
}
