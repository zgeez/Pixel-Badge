package com.ziadg.badge_app

import org.junit.Assert.*
import org.junit.Test
import java.io.IOException

class PlaybackPumpTest {
    private class Output : PcmOutput {
        override val capacityFrames = 1600
        override var playedFrames = 0L
        var queued = 0
        var primed = false
        var stalled = false
        var padding = 0
        override fun write(pcm: ByteArray, offset: Int, count: Int): Int {
            val frames = minOf(count/2, capacityFrames-queued)
            if (frames > 0 && pcm.sliceArray(offset until offset+frames*2).all { it.toInt() == 0 }) padding += frames
            queued += frames
            if (queued >= capacityFrames) primed = true
            return frames*2
        }
        fun advance(ms: Long) {
            if (!primed || stalled) return
            val consumed = minOf(queued.toLong(), ms*16).toInt()
            queued -= consumed; playedFrames += consumed
        }
    }
    private class Harness(val output: Output = Output()) {
        var now = 0L
        var active = true
        val pump = PlaybackPump(output, { active }, { now }, { ms ->
            now += ms; output.advance(ms)
        })
    }
    @Test fun finalTenMillisecondsStartsBelowThePlatformThreshold() {
        val h = Harness()
        assertTrue(h.pump.write(ByteArray(320) { 1 }))
        assertFalse(h.output.primed)
        assertTrue(h.pump.finish(160))
        assertTrue(h.output.primed)
        assertEquals(1440, h.output.padding)
        assertEquals(160L, h.output.playedFrames)
        assertEquals(10L, h.now)
    }
    @Test fun fullClipDrainsWithoutCountingSilenceAsAudio() {
        val h = Harness()
        repeat(100) { assertTrue(h.pump.write(ByteArray(320) { 1 })) }
        assertTrue(h.pump.finish(16000))
        assertTrue(h.output.playedFrames >= 16000)
        assertTrue(h.now in 1000L..1010L)
    }
    @Test fun stalledFinalDrainFailsWithinBudget() {
        val h = Harness()
        h.output.stalled = true
        assertTrue(h.pump.write(ByteArray(320) { 1 }))
        try { h.pump.finish(160); fail("Drain waited forever") }
        catch (_: IOException) {}
        assertEquals(2000L, h.now)
    }
    @Test fun fullOutputThatStopsAcceptingWritesFailsWithinBudget() {
        val h = Harness()
        h.output.stalled = true
        assertTrue(h.pump.write(ByteArray(3200) { 1 }))
        try { h.pump.write(ByteArray(320) { 1 }); fail("Write waited forever") }
        catch (_: IOException) {}
        assertEquals(2000L, h.now)
    }
    @Test fun cancellationAndEndSeekDoNotAddPaddingOrWait() {
        val h = Harness()
        assertTrue(h.pump.finish(0))
        assertEquals(0, h.output.padding)
        h.active = false
        assertFalse(h.pump.write(ByteArray(320)))
        assertFalse(h.pump.finish(160))
        assertEquals(0, h.output.padding)
        assertEquals(0L, h.now)
    }
}
