package com.ziadg.badge_app

import java.security.Key
import javax.crypto.Cipher
import javax.crypto.Mac
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

/** Pure JVM wire operations, shared by the Android service and vector tests. */
object AudioWire {
    const val storageReserve = 16L * 1024 * 1024
    const val maxBlocks = (0xffffffffL - 36L) / 320L
    fun wavBytes(blocks: Long): Long {
        require(blocks in 0..maxBlocks)
        return blocks * 320L
    }
    fun mac(key: Key, data: ByteArray): ByteArray = Mac.getInstance("HmacSHA256").run {
        init(key); doFinal(data)
    }
    fun decrypt(key: ByteArray, nonce: ByteArray, packet: ByteArray): ByteArray {
        require(key.size == 32 && nonce.size == 16 && packet.size == 105 && packet[0].toInt() == 1)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, SecretKeySpec(key, "AES"),
            GCMParameterSpec(128, nonce.copyOfRange(0, 8) + packet.copyOfRange(1, 5)))
        cipher.updateAAD("PB3A".toByteArray() + nonce + packet.copyOfRange(0, 5))
        return cipher.doFinal(packet.copyOfRange(5, 105))
    }
    private val steps = intArrayOf(7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767)
    fun decode(block: ByteArray): ByteArray {
        require(block.size == 84 && block[2].toInt() == 0 && block[3].toInt() == 0)
        var predictor = ((block[0].toInt() and 255) or (block[1].toInt() shl 8)).toShort().toInt()
        var index = 0
        val pcm = ByteArray(320)
        val changes = intArrayOf(-1,-1,-1,-1,2,4,6,8)
        repeat(160) { i ->
            val nibble = (block[4 + i / 2].toInt() ushr ((i % 2) * 4)) and 15
            val step = steps[index]
            var difference = step ushr 3
            if (nibble and 4 != 0) difference += step
            if (nibble and 2 != 0) difference += step ushr 1
            if (nibble and 1 != 0) difference += step ushr 2
            predictor = (predictor + if (nibble and 8 != 0) -difference else difference).coerceIn(-32768,32767)
            index = (index + changes[nibble and 7]).coerceIn(0,88)
            pcm[2 * i] = predictor.toByte(); pcm[2 * i + 1] = (predictor shr 8).toByte()
        }
        return pcm
    }
}
