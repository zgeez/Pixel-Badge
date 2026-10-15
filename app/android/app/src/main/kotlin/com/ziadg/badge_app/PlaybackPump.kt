package com.ziadg.badge_app

import java.io.IOException

internal interface PcmOutput {
    val capacityFrames: Int
    val playedFrames: Long
    fun write(pcm: ByteArray, offset: Int, count: Int): Int
}

/** Bounded nonblocking PCM writes and end-of-stream draining, independent of Android. */
internal class PlaybackPump(
    private val output: PcmOutput,
    private val active: () -> Boolean,
    private val nowMs: () -> Long,
    private val sleepMs: (Long) -> Unit = { Thread.sleep(it) },
    private val stallMs: Long = 2000L,
) {
    init { require(output.capacityFrames > 0 && stallMs > 0) }
    fun write(pcm: ByteArray, count: Int = pcm.size): Boolean {
        require(count in 0..pcm.size && count % 2 == 0)
        var offset = 0
        var deadline = nowMs() + stallMs
        while (offset < count && active()) {
            val written = output.write(pcm, offset, count-offset)
            if (written < 0 || written > count-offset || written % 2 != 0) throw IOException("PCM write failed")
            if (written > 0) {
                offset += written
                deadline = nowMs() + stallMs
            } else {
                if (nowMs() >= deadline) throw IOException("Playback write stalled")
                sleepMs(5)
            }
        }
        return offset == count && active()
    }
    fun finish(totalFrames: Long): Boolean {
        require(totalFrames >= 0)
        // A fresh/underrun stream may need a full buffer before playback begins.
        // Add only silence, then stop at the actual audio endpoint, not the padding.
        val pending = (totalFrames-output.playedFrames).coerceAtLeast(0)
        if (pending > 0) {
            var padding = (output.capacityFrames-pending).coerceAtLeast(0)
            val silence = ByteArray(320)
            while (padding > 0 && active()) {
                val frames = minOf(padding, 160L).toInt()
                if (!write(silence, frames*2)) return false
                padding -= frames
            }
        }
        var previous = output.playedFrames
        var deadline = nowMs() + stallMs
        while (active()) {
            val played = output.playedFrames
            if (played >= totalFrames) return true
            if (played > previous) {
                previous = played
                deadline = nowMs() + stallMs
            } else if (nowMs() >= deadline) throw IOException("Playback drain stalled")
            sleepMs(10)
        }
        return false
    }
}
