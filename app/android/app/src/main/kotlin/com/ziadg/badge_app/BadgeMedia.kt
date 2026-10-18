// SPDX-License-Identifier: MIT
package com.ziadg.badge_app

import android.app.Activity
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.media.MediaMetadataRetriever
import android.net.Uri
import android.os.Build
import android.os.Handler
import android.os.Looper
import io.flutter.plugin.common.MethodChannel
import java.lang.ref.WeakReference
import java.util.concurrent.Executors
import kotlin.math.*

/** Bounded editor work has its own executor, never the recording packet queue. */
class BadgeMedia(private val context: Context, channel: MethodChannel) {
    private val io = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    private var activity = WeakReference<Activity>(null)
    private var exportResult: MethodChannel.Result? = null
    private var exportBytes: ByteArray? = null
    fun attachActivity(value: Activity?) { activity = WeakReference(value) }
    private fun source(path: String): MediaMetadataRetriever {
        val reader = MediaMetadataRetriever()
        try { reader.setDataSource(context, Uri.parse(path).let { if(it.scheme==null) Uri.fromFile(java.io.File(path)) else it }); return reader }
        catch(error: Exception) { reader.release(); throw error }
    }
    private fun metadata(reader: MediaMetadataRetriever): Triple<Long,Int,Int> {
        val duration = reader.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull() ?: 0
        val width = reader.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH)?.toIntOrNull() ?: 0
        val height = reader.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT)?.toIntOrNull() ?: 0
        require(duration>0 && width>0 && height>0 && width.toLong()*height<=4096L*4096)
        if(Build.VERSION.SDK_INT<27) require(width.toLong()*height<=1920L*1080)
        return Triple(duration,width,height)
    }
    init {
        channel.setMethodCallHandler { call,result ->
            if(call.method=="export") {
                if (exportResult != null) { result.error("editor", "Finish the current export first.", null); return@setMethodCallHandler }
                try {
                    val bytes=call.argument<ByteArray>("bytes") ?: error("Missing data")
                    require(bytes.size<=4194304)
                    val mime=call.argument<String>("mime") ?: "application/json"
                    require(mime=="image/png" || mime=="application/json")
                    val target=activity.get() ?: error("Open editor")
                    exportBytes=bytes.copyOf();exportResult=result
                    target.startActivityForResult(Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                        addCategory(Intent.CATEGORY_OPENABLE);type=mime
                        putExtra(Intent.EXTRA_TITLE,if(mime=="image/png")"pixel-badge.png" else "pixel-badge.json")
                    },9083)
                } catch(_: Exception) { exportBytes=null;exportResult=null;result.error("editor","Could not open export picker.",null) }
                return@setMethodCallHandler
            }
            if(call.method !in listOf("videoInfo","videoFrames","clipboardImage")) {result.notImplemented();return@setMethodCallHandler}
            // Clipboard access is foreground/main-thread work; stream decode is not.
            val imageUri = try { if(call.method=="clipboardImage") {
                val clip=(context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager).primaryClip
                if(clip!=null && clip.itemCount>0)clip.getItemAt(0).uri else null
            } else null } catch (_: Exception) { result.error("editor", "Could not read the clipboard image.", null); return@setMethodCallHandler }
            io.execute {
                try {
                    val value: Any?=when(call.method) {
                        "clipboardImage" -> {
                            if(imageUri==null)null else {
                                require(context.contentResolver.getType(imageUri)?.startsWith("image/")==true)
                                // Stream to a temporary file rather than carrying the
                                // whole source through Dart/platform-channel memory.
                                val file=java.io.File.createTempFile("badge-import-", ".image", context.cacheDir)
                                try {
                                    context.contentResolver.openInputStream(imageUri)!!.use { input ->
                                        file.outputStream().use { output -> input.copyTo(output) }
                                    }
                                    require(file.length()>0)
                                    file.absolutePath
                                } catch(error: Exception) {file.delete();throw error}
                            }
                        }
                        else -> {
                            val reader=source(call.argument<String>("path") ?: error("Missing source"))
                            try {
                                val (duration,width,height)=metadata(reader)
                                if(call.method=="videoInfo")mapOf("duration" to duration,"width" to width,"height" to height)
                                else {
                                    val start=call.argument<Number>("start")!!.toLong()
                                    val end=call.argument<Number>("end")!!.toLong()
                                    val fps=call.argument<Number>("fps")!!.toInt()
                                    val sampling=MediaSampling.plan(duration,start,end,fps)
                                    val count=sampling.first
                                    val interval=sampling.second
                                    val out=ArrayList<Map<String,Any>>()
                                    for(i in 0 until count) {
                                        val time=(start+(end-start)*i/count)*1000
                                        val w=if(width>=height)64 else max(1,64*width/height)
                                        val h=if(height>=width)64 else max(1,64*height/width)
                                        val original=if(Build.VERSION.SDK_INT>=27)reader.getScaledFrameAtTime(time,MediaMetadataRetriever.OPTION_CLOSEST,w,h)
                                            else reader.getFrameAtTime(time,MediaMetadataRetriever.OPTION_CLOSEST)
                                        check(original!=null)
                                        val bitmap=Bitmap.createScaledBitmap(original,w,h,true)
                                        try {
                                            val pixels=IntArray(w*h);bitmap.getPixels(pixels,0,w,0,0,w,h)
                                            val bytes=MediaSampling.rgba(pixels)
                                            out.add(mapOf("width" to w,"height" to h,"duration" to interval,"pixels" to bytes))
                                        } finally {if(bitmap!==original)bitmap.recycle();original.recycle()}
                                    }
                                    out
                                }
                            } finally {reader.release()}
                        }
                    }
                    main.post{result.success(value)}
                } catch(_: Exception) { main.post{result.error("editor","Could not import this media. Try a shorter clip or PNG/GIF.",null)} }
            }
        }
    }
    fun exportResult(requestCode:Int,resultCode:Int,data:Intent?):Boolean {
        if(requestCode!=9083)return false
        val result=exportResult;val bytes=exportBytes;exportResult=null;exportBytes=null
        val uri=data?.data
        if(resultCode!=Activity.RESULT_OK || uri==null || bytes==null){result?.success(false);return true}
        io.execute {
            try{context.contentResolver.openOutputStream(uri,"wt")!!.use{it.write(bytes)};main.post{result?.success(true)}}
            catch(_:Exception){main.post{result?.error("editor","Could not export this design.",null)}}
        }
        return true
    }
}
