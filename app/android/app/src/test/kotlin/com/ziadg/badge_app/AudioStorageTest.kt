package com.ziadg.badge_app

import org.junit.Assert.*
import org.junit.Test
import javax.crypto.Cipher
import javax.crypto.spec.SecretKeySpec

class AudioStorageTest {
    private val master=SecretKeySpec(ByteArray(32) { 3 },"AES")
    private val fileKey=ByteArray(32) { 7 }
    private val name="12345678-12345678.pba"
    private fun rejected(work: ()->Unit) {
        try { work(); fail("Unauthenticated data accepted") }
        catch (_: javax.crypto.AEADBadTagException) {}
    }
    @Test fun wrappedKeyAndChunksBindToFileAndSequence() {
        val header=AudioStorage.wrap(master,name,fileKey)
        assertEquals(64,header.size)
        assertArrayEquals(fileKey,AudioStorage.unwrap(master,name,header))
        rejected { AudioStorage.unwrap(master,"other.pba",header) }
        rejected { AudioStorage.unwrap(master,name,header.copyOf().also { it[63]=(it[63].toInt() xor 1).toByte() }) }
        val chunk=AudioStorage.seal(fileKey,name,123,ByteArray(84))
        val key=SecretKeySpec(fileKey,"AES")
        assertEquals(112,chunk.size)
        assertArrayEquals(ByteArray(84),AudioStorage.open(key,name,123,chunk))
        rejected { AudioStorage.open(key,name,124,chunk) }
        rejected { AudioStorage.open(key,"other.pba",123,chunk) }
        rejected { AudioStorage.open(key,name,123,chunk.copyOf().also { it[111]=(it[111].toInt() xor 1).toByte() }) }
    }
    @Test fun oldFilesRemainReadableWithoutAHeader() {
        val cipher=Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE,master)
        cipher.updateAAD("PB3F".toByteArray()+name.toByteArray()+ByteArray(4))
        val chunk=cipher.iv+cipher.doFinal(ByteArray(84))
        assertEquals(0L,AudioStorage.headerBytes(chunk.size.toLong()))
        assertEquals(1L,AudioStorage.blockCount(chunk.size.toLong()))
        assertArrayEquals(ByteArray(84),AudioStorage.open(master,name,0,chunk))
    }
    @Test fun versionedLengthAndLongRecordings() {
        assertEquals(64L,AudioStorage.headerBytes(64L+112L*30001))
        assertEquals(30001L,AudioStorage.blockCount(64L+112L*30001))
        try { AudioStorage.blockCount(65); fail("Incomplete chunk accepted") }
        catch (_: IllegalStateException) {}
        try { AudioStorage.blockCount(64L+112L*(AudioWire.maxBlocks+1)); fail("WAV overflow accepted") }
        catch (_: IllegalArgumentException) {}
    }
}
