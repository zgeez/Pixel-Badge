package com.ziadg.badge_app

import org.junit.Assert.*
import org.junit.Test
import java.io.File
import java.io.RandomAccessFile
import java.nio.file.Files
import javax.crypto.Cipher
import javax.crypto.spec.SecretKeySpec

class RecordingReaderTest {
    private val master = SecretKeySpec(ByteArray(32) { 3 }, "AES")
    private val raw = ByteArray(32) { 7 }
    private fun fixture(legacy: Boolean, test: (File) -> Unit) {
        val dir = Files.createTempDirectory("badge-player").toFile()
        val file = File(dir, "12345678-12345678.pba")
        try {
            file.outputStream().use { out ->
                if (!legacy) out.write(AudioStorage.wrap(master, file.name, raw))
                repeat(30) { index ->
                    val block = ByteArray(84).also { it[0] = index.toByte() }
                    if (legacy) {
                        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
                        cipher.init(Cipher.ENCRYPT_MODE, master)
                        cipher.updateAAD("PB3F".toByteArray() + file.name.toByteArray() + ByteArray(4) { (index ushr (8*it)).toByte() })
                        out.write(cipher.iv + cipher.doFinal(block))
                    } else out.write(AudioStorage.seal(raw, file.name, index.toLong(), block))
                }
            }
            test(file)
        } finally { file.delete(); dir.delete() }
    }
    @Test fun forwardBackwardAndLastBlockMatchSequentialDecode() {
        for (legacy in listOf(false, true)) fixture(legacy) { file ->
            RecordingReader(file, master).use { reader ->
                assertEquals(30L, reader.blocks)
                for (index in listOf(29L, 0L, 15L, 1L, 29L)) {
                    val expected = AudioWire.decode(ByteArray(84).also { it[0] = index.toByte() })
                    assertArrayEquals(expected, reader.read(index))
                }
                for (bad in listOf(-1L, 30L)) {
                    try { reader.read(bad); fail("Out-of-range read accepted") } catch (_: IllegalArgumentException) {}
                }
            }
        }
    }
    @Test fun seekStillAuthenticatesTheSelectedBlock() = fixture(false) { file ->
        RandomAccessFile(file, "rw").use { it.seek(64L + 112L*29 + 111); it.write(42) }
        RecordingReader(file, master).use { reader ->
            assertEquals(320, reader.read(0).size)
            try { reader.read(29); fail("Tampered block played") } catch (_: javax.crypto.AEADBadTagException) {}
        }
    }
    @Test fun incompleteFileIsRejectedBeforePlayback() = fixture(false) { file ->
        RandomAccessFile(file, "rw").use { it.setLength(it.length()-1) }
        try { RecordingReader(file, master).close(); fail("Truncated file played") } catch (_: IllegalStateException) {}
    }
}
