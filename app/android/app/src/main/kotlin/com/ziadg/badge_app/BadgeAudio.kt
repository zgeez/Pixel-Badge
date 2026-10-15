package com.ziadg.badge_app

import android.app.Activity
import android.content.Intent
import android.content.Context
import java.lang.ref.WeakReference
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import io.flutter.plugin.common.MethodChannel
import java.io.*
import java.math.BigInteger
import java.security.*
import java.security.interfaces.ECPublicKey
import java.security.spec.*
import java.util.concurrent.Executors
import javax.crypto.*
import javax.crypto.spec.*

/** Secrets and plaintext never cross the Flutter channel. Recordings are chunk
 * encrypted in noBackupFilesDir; export uses the explicit system document picker.
 * All crypto/file operations share one serial executor with a bounded input flow. */
class BadgeAudio(private val context: Context, channel: MethodChannel) {
    private var activity = WeakReference<Activity>(null)
    fun attachActivity(value: Activity?) { activity = WeakReference(value) }
    private val io = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    private val keys = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
    private val dir = File(context.noBackupFilesDir, "recordings-v1").apply { mkdirs() }
    private var pair: KeyPair? = null
    private var connectionKey: ByteArray? = null
    private var session: ByteArray? = null
    private var recordingNonce: ByteArray? = null
    private var fileKey: ByteArray? = null
    private var sequence = 0L
    @Volatile private var stream: DataOutputStream? = null
    private var file: File? = null
    private val player = RecordingPlayer(context) { storageKey() }
    private var exportFile: File? = null
    private var exportResult: MethodChannel.Result? = null
    private fun mac(key: java.security.Key, data: ByteArray): ByteArray = AudioWire.mac(key,data)
    private fun mac(key: ByteArray, data: ByteArray) = mac(SecretKeySpec(key,"HmacSHA256"),data)
    private fun unsigned(value: BigInteger) = value.toByteArray().let { bytes -> ByteArray(32).also { out -> val count=minOf(32,bytes.size); bytes.copyInto(out,32-count,bytes.size-count) } }
    private fun publicBytes(key: ECPublicKey) = byteArrayOf(4) + unsigned(key.w.affineX) + unsigned(key.w.affineY)
    @Synchronized private fun storageKey(): java.security.Key {
        if (!keys.containsAlias("pixel-badge-recordings-v1")) {
            KeyGenerator.getInstance("AES","AndroidKeyStore").apply {
                init(KeyGenParameterSpec.Builder("pixel-badge-recordings-v1",KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                    .setKeySize(256).setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).build())
                generateKey()
            }
        }
        return keys.getKey("pixel-badge-recordings-v1",null)
    }
    private fun selected(id: String): File {
        require(Regex("[0-9]+-[a-f0-9]{8}\\.pba").matches(id))
        return File(dir,id)
    }
    private fun finish(): String? {
        val target = file
        val output = stream; stream=null; file=null
        try { output?.close() } finally {
            session?.fill(0); session=null; recordingNonce?.fill(0); recordingNonce=null; fileKey?.fill(0); fileKey=null; sequence=0
            if (target?.isFile == true) {
                // Preserve complete authenticated chunks after a short/failed write.
                if (target.length() >= AudioStorage.headerSize) {
                    RandomAccessFile(target,"rw").use { it.setLength(AudioStorage.headerSize + (it.length()-AudioStorage.headerSize)/112L*112L) }
                }
                if (target.length() <= AudioStorage.headerSize) target.delete()
            }
        }
        return if (target?.isFile == true) target.name else null
    }
    init {
        channel.setMethodCallHandler { call,result ->
            if (call.method=="export") {
                try {
                    check(exportResult==null && stream==null)
                    val target=selected(call.argument<String>("id")!!); check(target.isFile)
                    exportFile=target; exportResult=result
                    (activity.get() ?: error("Open the app to export")).startActivityForResult(Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                        addCategory(Intent.CATEGORY_OPENABLE); type="audio/wav"; putExtra(Intent.EXTRA_TITLE,target.nameWithoutExtension+".wav")
                    },7314)
                } catch (_: Exception) { exportResult=null; exportFile=null; result.error("audio","Could not open export picker.",null) }
                return@setMethodCallHandler
            }
            if (call.method in listOf("play", "pausePlayback", "resumePlayback", "seekPlayback", "stopPlayback", "playbackState")) {
                try {
                    when (call.method) {
                        "play" -> { check(stream == null); player.play(selected(call.argument<String>("id")!!)) }
                        "pausePlayback" -> player.pause()
                        "resumePlayback" -> { check(stream == null); player.resume() }
                        "seekPlayback" -> player.seek(call.argument<Number>("positionMs")!!.toLong())
                        "stopPlayback" -> player.stop()
                    }
                    result.success(player.state())
                } catch (_: Exception) { result.error("playback", "Could not play this recording.", null) }
                return@setMethodCallHandler
            }
            io.execute {
                try {
                    val value: Any? = when(call.method) {
                        "beginSession" -> {
                            check(stream==null)
                            connectionKey?.fill(0); connectionKey=null
                            pair=KeyPairGenerator.getInstance("EC").apply { initialize(ECGenParameterSpec("secp256r1")) }.generateKeyPair()
                            publicBytes(pair!!.public as ECPublicKey)
                        }
                        "deriveSession" -> {
                            val pub=call.argument<ByteArray>("public")!!; val nonce=call.argument<ByteArray>("nonce")!!
                            require(pub.size==65 && pub[0].toInt()==4 && nonce.size==16)
                            val ours=pair ?: error("Reconnect first")
                            val params=(ours.public as ECPublicKey).params
                            val peer=KeyFactory.getInstance("EC").generatePublic(ECPublicKeySpec(ECPoint(BigInteger(1,pub.copyOfRange(1,33)),BigInteger(1,pub.copyOfRange(33,65))),params))
                            val secret=KeyAgreement.getInstance("ECDH").run { init(ours.private); doPhase(peer,true); generateSecret() }
                            try {
                                val prk=mac(nonce,secret)
                                try { connectionKey=mac(prk,"PB3E".toByteArray()+publicBytes(ours.public as ECPublicKey)+pub+byteArrayOf(1)) }
                                finally { prk.fill(0) }
                            } finally { secret.fill(0); pair=null }
                            null
                        }
                        "endSession" -> { connectionKey?.fill(0); connectionKey=null; pair=null; null }
                        "control" -> {
                            val body=call.argument<ByteArray>("body")!!; val nonce=call.argument<ByteArray>("nonce")!!
                            require(body.size==7 && nonce.size==16)
                            val extra=call.argument<ByteArray>("challenge") ?: byteArrayOf()
                            require(extra.size==0 || (body[0].toInt()==5 && extra.size==16))
                            val controlKey = connectionKey ?: error("Reconnect first")
                            val digest=mac(controlKey,"PB3C".toByteArray()+nonce+body+extra)
                            (body+digest.copyOfRange(0,16)).also { digest.fill(0) }
                        }
                        "prepareRecording" -> {
                            player.stop()
                            check(stream==null)
                            if (dir.usableSpace < AudioWire.storageReserve) throw AudioCapacityException("Phone storage is low.")
                            val name="${System.currentTimeMillis()}-${java.util.UUID.randomUUID().toString().take(8)}.pba"
                            fileKey=ByteArray(32).also { SecureRandom().nextBytes(it) }
                            val header=AudioStorage.wrap(storageKey(),name,fileKey!!)
                            file=File(dir,name)
                            stream=DataOutputStream(BufferedOutputStream(FileOutputStream(file)))
                            stream!!.write(header); null
                        }
                        "beginRecording" -> {
                            check(stream!=null && fileKey!=null && session==null)
                            val nonce=call.argument<ByteArray>("nonce")!!; require(nonce.size==16)
                            session=mac(connectionKey ?: error("Reconnect first"),"PB3R".toByteArray()+nonce)
                            recordingNonce=nonce.copyOf(); sequence=0; null
                        }
                        "packet" -> {
                            val packet=call.arguments as ByteArray; require(packet.size==105 && packet[0].toInt()==1)
                            val seq=(0..3).sumOf { (packet[it+1].toLong() and 255) shl (8*it) }
                            require(seq==sequence)
                            if (seq >= AudioWire.maxBlocks) throw AudioCapacityException("Recording reached the WAV file limit.")
                            if (seq % 100L == 0L && dir.usableSpace < AudioWire.storageReserve) throw AudioCapacityException("Phone storage is low; recording stopped.")
                            val nonce=recordingNonce ?: error("Not recording")
                            val block=AudioWire.decrypt(session!!,nonce,packet)
                            try {
                                require(block.size==84 && block[2].toInt()==0 && block[3].toInt()==0)
                                val encrypted=AudioStorage.seal(fileKey!!,file!!.name,seq,block)
                                stream!!.write(encrypted); sequence++; null
                            } finally { block.fill(0) }
                        }
                        "finishRecording" -> finish()
                        "list" -> dir.listFiles()?.filter { it.extension=="pba" }?.sortedByDescending { it.name }?.map { mapOf("id" to it.name,"seconds" to (AudioStorage.blockCount(it.length())/100.0)) } ?: emptyList<Any>()
                        "delete" -> { check(stream==null); player.stop(); selected(call.argument<String>("id")!!).delete(); null }
                        else -> error("Unsupported audio operation")
                    }
                    main.post { result.success(value) }
                } catch (failure: Exception) {
                    // Generic diagnostics: never include packet bytes, keys or audio.
                    try { finish() } catch (_: Exception) {}
                    connectionKey?.fill(0); connectionKey=null; pair=null
                    main.post { result.error(if (failure is AudioCapacityException) "capacity" else "audio", if (failure is AudioCapacityException) failure.message else "Audio operation failed. Reconnect.",null) }
                }
            }
        }
    }
    private fun readBlocks(recording: File, consume: (ByteArray)->Boolean) {
        val count=AudioStorage.blockCount(recording.length())
        val headerSize=AudioStorage.headerBytes(recording.length())
        val master=storageKey()
        DataInputStream(BufferedInputStream(FileInputStream(recording))).use { input ->
            var rawKey: ByteArray? = null
            try {
                val key = if (headerSize != 0L) {
                    val header=ByteArray(headerSize.toInt()); input.readFully(header)
                    rawKey=AudioStorage.unwrap(master,recording.name,header)
                    SecretKeySpec(rawKey,"AES")
                } else master
                repeat(count.toInt()) { index ->
                    val chunk=ByteArray(112); input.readFully(chunk)
                    val block=AudioStorage.open(key,recording.name,index.toLong(),chunk)
                    val pcm=try { AudioWire.decode(block) } finally { block.fill(0) }
                    val more=try { consume(pcm) } finally { pcm.fill(0) }
                    if(!more) return
                }
            } finally { rawKey?.fill(0) }
        }
    }
    fun exportResult(requestCode: Int, resultCode: Int, data: Intent?): Boolean {
        if(requestCode!=7314) return false
        val result=exportResult; val recording=exportFile; exportResult=null; exportFile=null
        val uri: Uri?=data?.data
        if(resultCode!=Activity.RESULT_OK || uri==null || recording==null) { result?.success(null); return true }
        io.execute {
            try {
                context.contentResolver.openOutputStream(uri,"wt")!!.use { out ->
                    val size=AudioWire.wavBytes(AudioStorage.blockCount(recording.length())).toInt()
                    fun le(value:Int,count:Int)=ByteArray(count) { (value ushr (8*it)).toByte() }
                    out.write("RIFF".toByteArray()); out.write(le(size+36,4)); out.write("WAVEfmt ".toByteArray()); out.write(le(16,4)); out.write(le(1,2)); out.write(le(1,2)); out.write(le(16000,4)); out.write(le(32000,4)); out.write(le(2,2)); out.write(le(16,2)); out.write("data".toByteArray()); out.write(le(size,4))
                    readBlocks(recording) { pcm -> out.write(pcm); true }
                }
                main.post { result?.success(null) }
            } catch (_: Exception) { main.post { result?.error("audio","Export failed.",null) } }
        }
        return true
    }
    fun close() { player.close(); io.execute { try { finish() } catch (_: Exception) {}; connectionKey?.fill(0); connectionKey=null; pair=null }; io.shutdown() }
}

private class AudioCapacityException(message: String): IOException(message)
