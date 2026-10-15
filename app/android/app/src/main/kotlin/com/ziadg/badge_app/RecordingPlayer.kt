package com.ziadg.badge_app

import android.content.Context
import android.media.AudioManager
import android.media.AudioFocusRequest
import android.os.Build
import android.os.SystemClock
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import java.io.File
import java.security.Key
import java.util.concurrent.Executors

/** Playback owns its executor: a long file cannot block BLE crypto or controls. */
internal class RecordingPlayer(context: Context, private val master: () -> Key) {
    private val manager = context.getSystemService(AudioManager::class.java)
    private val attributes = AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_SPEECH).build()
    private val focusListener = AudioManager.OnAudioFocusChangeListener { change ->
        if (change != AudioManager.AUDIOFOCUS_GAIN) pause()
    }
    private val focusRequest = if (Build.VERSION.SDK_INT >= 26)
        AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(attributes)
            .setOnAudioFocusChangeListener(focusListener).build() else null
    @Suppress("DEPRECATION")
    private fun focus(): Boolean = (if (Build.VERSION.SDK_INT >= 26) manager.requestAudioFocus(focusRequest!!)
        else manager.requestAudioFocus(focusListener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
    @Suppress("DEPRECATION")
    private fun abandonFocus() {
        if (Build.VERSION.SDK_INT >= 26) manager.abandonAudioFocusRequest(focusRequest!!)
        else manager.abandonAudioFocus(focusListener)
    }
    private val worker = Executors.newSingleThreadExecutor()
    private val lock = Any()
    private var epoch = 0L
    private var trackEpoch = -1L
    private var file: File? = null
    private var duration = 0L
    private var position = 0L
    private var base = 0L
    private var playing = false
    private var track: AudioTrack? = null
    private var error: String? = null
    private fun positionLocked(): Long = if (playing && track != null && trackEpoch == epoch)
        (base + (track!!.playbackHeadPosition.toLong() and 0xffffffffL) / 16L).coerceAtMost(duration)
        else position
    fun state(): Map<String, Any?> = synchronized(lock) {
        mapOf("id" to file?.name, "positionMs" to positionLocked(), "durationMs" to duration,
            "playing" to playing, "error" to error)
    }
    fun play(target: File) = synchronized(lock) {
        val total = AudioStorage.blockCount(target.length()) * 10L
        epoch++; file = target; duration = total
        position = 0; playing = focus(); error = if (playing) null else "Audio is in use by another app."; if (playing) schedule()
    }
    fun pause() = synchronized(lock) {
        position = positionLocked(); playing = false; epoch++; abandonFocus()
    }
    fun resume() = synchronized(lock) {
        if (file != null && !playing) {
            if (position >= duration) position = 0
            playing = focus(); error = if (playing) null else "Audio is in use by another app."; epoch++; if (playing) schedule()
        }
    }
    fun seek(ms: Long) = synchronized(lock) {
        position = ms.coerceIn(0, duration) / 10L * 10L; epoch++
        if (playing) schedule()
    }
    fun stop() = synchronized(lock) { epoch++; playing = false; position = 0; file = null; error = null; abandonFocus() }
    private fun active(version: Long) = synchronized(lock) { epoch == version && playing }
    private fun schedule() {
        val version = epoch; val target = file ?: return; val start = position / 10L * 10L
        worker.execute {
            var output: AudioTrack? = null
            try {
                if (!active(version)) return@execute
                RecordingReader(target, master()).use { reader ->
                    val minimum = AudioTrack.getMinBufferSize(16000, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT)
                    val built = AudioTrack.Builder()
                        .setAudioAttributes(attributes)
                        .setAudioFormat(AudioFormat.Builder().setSampleRate(16000).setChannelMask(AudioFormat.CHANNEL_OUT_MONO).setEncoding(AudioFormat.ENCODING_PCM_16BIT).build())
                        .setBufferSizeInBytes(maxOf(minimum, 3200)).setTransferMode(AudioTrack.MODE_STREAM).build()
                    output = built
                    synchronized(lock) {
                        if (epoch != version || !playing) return@execute
                        track = built; trackEpoch = version; base = start; built.play()
                    }
                    val pump = PlaybackPump(object : PcmOutput {
                        override val capacityFrames get() = built.bufferSizeInFrames
                        override val playedFrames get() = built.playbackHeadPosition.toLong() and 0xffffffffL
                        override fun write(pcm: ByteArray, offset: Int, count: Int) =
                            built.write(pcm, offset, count, AudioTrack.WRITE_NON_BLOCKING)
                    }, { active(version) }, { SystemClock.elapsedRealtime() })
                    var index = start / 10L
                    while (index < reader.blocks && active(version)) {
                        val pcm = reader.read(index++)
                        try { if (!pump.write(pcm)) return@execute }
                        finally { pcm.fill(0) }
                    }
                    if (pump.finish((reader.blocks-start/10L)*160L)) synchronized(lock) {
                        if (epoch == version) { position = duration; playing = false; abandonFocus() }
                    }
                }
            } catch (_: Exception) {
                synchronized(lock) {
                    if (epoch == version) {
                        position = positionLocked(); playing = false; abandonFocus()
                        error = "Could not play this recording."
                    }
                }
            } finally {
                synchronized(lock) {
                    if (track === output) track = null
                    try { output?.pause(); output?.flush() } catch (_: Exception) {}
                    output?.release()
                }
            }
        }
    }
    fun close() { stop(); worker.shutdown() }
}
