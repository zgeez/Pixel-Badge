package com.ziadg.badge_app

import java.security.Key
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

/** V2 wraps a fresh file key once using Keystore. Packet encryption is local. */
object AudioStorage {
    const val headerSize = 64L
    private val magic = "PBA2".toByteArray()
    fun headerBytes(length: Long): Long {
        require(length >= 0)
        return when (length % 112L) {
            0L -> 0L // Existing V1 files remain readable.
            headerSize -> headerSize
            else -> error("Incomplete recording")
        }
    }
    fun blockCount(length: Long): Long {
        val count = (length - headerBytes(length)) / 112L
        require(count in 0..AudioWire.maxBlocks)
        return count
    }
    fun wrap(master: Key, name: String, key: ByteArray): ByteArray {
        require(key.size == 32)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, master)
        cipher.updateAAD("PB3K".toByteArray() + name.toByteArray())
        return magic + cipher.iv + cipher.doFinal(key)
    }
    fun unwrap(master: Key, name: String, header: ByteArray): ByteArray {
        require(header.size == headerSize.toInt() && header.copyOfRange(0,4).contentEquals(magic))
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, master, GCMParameterSpec(128, header.copyOfRange(4,16)))
        cipher.updateAAD("PB3K".toByteArray() + name.toByteArray())
        return cipher.doFinal(header.copyOfRange(16,64)).also { require(it.size == 32) }
    }
    private fun count(sequence: Long): ByteArray {
        require(sequence in 0 until AudioWire.maxBlocks)
        return ByteArray(4) { (sequence ushr (8*it)).toByte() }
    }
    fun seal(key: ByteArray, name: String, sequence: Long, block: ByteArray): ByteArray {
        require(key.size == 32 && block.size == 84)
        val number = count(sequence)
        // Unique counter IV under a newly generated key for each file.
        val iv = ByteArray(8) + number
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, SecretKeySpec(key,"AES"), GCMParameterSpec(128,iv))
        cipher.updateAAD("PB3F".toByteArray() + name.toByteArray() + number)
        return iv + cipher.doFinal(block)
    }
    fun open(key: Key, name: String, sequence: Long, chunk: ByteArray): ByteArray {
        require(chunk.size == 112)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(128,chunk.copyOfRange(0,12)))
        cipher.updateAAD("PB3F".toByteArray() + name.toByteArray() + count(sequence))
        return cipher.doFinal(chunk.copyOfRange(12,112))
    }
}
