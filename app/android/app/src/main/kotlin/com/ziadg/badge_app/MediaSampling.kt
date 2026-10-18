// SPDX-License-Identifier: MIT
package com.ziadg.badge_app
import kotlin.math.*
internal object MediaSampling {
    fun plan(duration: Long, start: Long, end: Long, fps: Int): Pair<Int,Int> {
        require(start >= 0 && end > start && end <= duration && end-start <= 64000 && fps in 5..20)
        val count=min(128,max(1,ceil((end-start)*fps/1000.0).toInt()))
        val interval=max(50,ceil((end-start).toDouble()/count).toInt())
        require(interval<=2000)
        return Pair(count,interval)
    }
    fun rgba(pixels: IntArray): ByteArray {
        val bytes=ByteArray(pixels.size*4)
        for(p in pixels.indices) {
            val color=pixels[p]
            bytes[p*4]=(color ushr 16).toByte();bytes[p*4+1]=(color ushr 8).toByte()
            bytes[p*4+2]=color.toByte();bytes[p*4+3]=(color ushr 24).toByte()
        }
        return bytes
    }
}
