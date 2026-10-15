package com.ziadg.badge_app

import org.junit.Assert.*
import org.junit.Test
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

class AudioWireTest {
    @Test fun recordingsBeyondFiveMinutesAndWavBoundary() {
        assertEquals(9600320L, AudioWire.wavBytes(30001))
        assertTrue(AudioWire.wavBytes(AudioWire.maxBlocks) + 36 <= 0xffffffffL)
        try { AudioWire.wavBytes(AudioWire.maxBlocks + 1); fail("WAV overflow accepted") }
        catch (_: IllegalArgumentException) {}
    }
    @Test fun hmacRfc4231() {
        val key = ByteArray(32) { if (it < 20) 11 else 0 }
        val digest = AudioWire.mac(SecretKeySpec(key,"HmacSHA256"),"Hi There".toByteArray())
        assertEquals("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
            digest.joinToString("") { "%02x".format(it) })
    }
    @Test fun voiceNibbleOrderAndSilence() {
        val block = ByteArray(84); block[4] = 0xf7.toByte()
        val pcm = AudioWire.decode(block)
        fun sample(i: Int) = ((pcm[2*i].toInt() and 255) or (pcm[2*i+1].toInt() shl 8)).toShort().toInt()
        assertEquals(11,sample(0)); assertEquals(-19,sample(1))
        assertArrayEquals(ByteArray(320),AudioWire.decode(ByteArray(84)))
    }
    @Test fun recordingAadAndNonceRejectTamperingAndOtherSessions() {
        val key = ByteArray(32); val nonce = ByteArray(16); val header = byteArrayOf(1,0,0,0,0)
        val encrypt = Cipher.getInstance("AES/GCM/NoPadding")
        encrypt.init(Cipher.ENCRYPT_MODE,SecretKeySpec(key,"AES"),GCMParameterSpec(128,ByteArray(12)))
        encrypt.updateAAD("PB3A".toByteArray()+nonce+header)
        val packet = header + encrypt.doFinal(ByteArray(84))
        assertArrayEquals(ByteArray(84),AudioWire.decrypt(key,nonce,packet))
        fun rejected(candidate: ByteArray, recording: ByteArray) {
            try { AudioWire.decrypt(key,recording,candidate); fail("Unauthenticated packet accepted") }
            catch (_: javax.crypto.AEADBadTagException) {}
        }
        rejected(packet.copyOf().also { it[104] = (it[104].toInt() xor 1).toByte() },nonce)
        rejected(packet.copyOf().also { it[1] = 1 },nonce)
        rejected(packet,nonce.copyOf().also { it[15] = 1 })
    }
}
