// SPDX-License-Identifier: MIT
package com.ziadg.badge_app
import org.junit.Assert.*
import org.junit.Test
class MediaSamplingTest {
    @Test fun samplingBoundsPreserveClipDuration() {
        assertEquals(Pair(128,500),MediaSampling.plan(100000,1000,65000,20))
        assertEquals(Pair(10,100),MediaSampling.plan(1000,0,1000,10))
        assertEquals(Pair(1,50),MediaSampling.plan(50,0,50,20))
        for ((start,end,fps) in listOf(Triple(-1L,50L,10),Triple(0L,0L,10),Triple(0L,64001L,10),Triple(0L,50L,21))) {
            try { MediaSampling.plan(100000,start,end,fps);fail("invalid clip accepted") } catch (_:IllegalArgumentException) {}
        }
    }
    @Test fun rgbaPreservesChannelsAndAlpha() {
        assertArrayEquals(byteArrayOf(0x12,0x34,0x56,0x7f,0xff.toByte(),0,0,0xff.toByte()),MediaSampling.rgba(intArrayOf(0x7f123456,0xffff0000.toInt())))
    }
}
