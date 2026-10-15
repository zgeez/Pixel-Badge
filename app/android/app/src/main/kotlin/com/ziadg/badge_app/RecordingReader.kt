package com.ziadg.badge_app

import java.io.File
import java.io.RandomAccessFile
import java.security.Key
import javax.crypto.spec.SecretKeySpec

/** Independent authenticated 10ms blocks permit bounded-memory random access. */
internal class RecordingReader(file: File, master: Key): AutoCloseable {
    private val input = RandomAccessFile(file, "r")
    val blocks: Long
    private val header: Long
    private var rawKey: ByteArray? = null
    private val key: Key
    private val name = file.name
    init {
        try {
            blocks = AudioStorage.blockCount(input.length())
            header = AudioStorage.headerBytes(input.length())
            key = if (header != 0L) {
                val bytes = ByteArray(header.toInt()); input.readFully(bytes)
                rawKey = AudioStorage.unwrap(master, name, bytes)
                SecretKeySpec(rawKey, "AES")
            } else master
        } catch (error: Exception) { input.close(); rawKey?.fill(0); throw error }
    }
    fun read(index: Long): ByteArray {
        require(index >= 0 && index < blocks)
        input.seek(header + index * 112L)
        val bytes = ByteArray(112); input.readFully(bytes)
        val block = AudioStorage.open(key, name, index, bytes)
        return try { AudioWire.decode(block) } finally { block.fill(0) }
    }
    override fun close() { try { input.close() } finally { rawKey?.fill(0) } }
}
