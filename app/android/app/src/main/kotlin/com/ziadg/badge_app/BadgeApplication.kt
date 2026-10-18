package com.ziadg.badge_app

import android.app.Application
import android.content.Intent
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.AtomicFile
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.embedding.engine.dart.DartExecutor
import io.flutter.plugin.common.MethodChannel
import java.io.File
import java.io.FileNotFoundException
import java.util.concurrent.Executors

/** The BLE engine outlives Activity recreation/Back while its service is active. */
class BadgeApplication : Application() {
    lateinit var audio: BadgeAudio
        private set
    lateinit var media: BadgeMedia
        private set
    private val connectionLease = ConnectionLease()
    private val draftIo = Executors.newSingleThreadExecutor()
    val engine: FlutterEngine by lazy {
        val flutterEngine = FlutterEngine(this)
        media = BadgeMedia(this, MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "com.ziadg.badge_app/editor"))
        audio = BadgeAudio(this, MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "com.ziadg.badge_app/audio"))
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "com.ziadg.badge_app/drafts")
            .setMethodCallHandler { call, result ->
                if (call.method != "read" && call.method != "write") {
                    result.notImplemented()
                    return@setMethodCallHandler
                }
                draftIo.execute {
                    try {
                        val file = AtomicFile(File(noBackupFilesDir, "studio-v1.json"))
                        val value = if (call.method == "read") {
                            // openRead restores an interrupted-write backup,
                            // including when the base file is temporarily absent.
                            val input = try { file.openRead() } catch (_: FileNotFoundException) { null }
                            input?.bufferedReader(Charsets.UTF_8)?.use {
                                require(file.baseFile.length() <= 16777216)
                                it.readText()
                            }
                        } else {
                            val text = call.arguments as? String ?: error("Invalid draft")
                            require(text.toByteArray(Charsets.UTF_8).size <= 16777216)
                            val stream = file.startWrite()
                            try {
                                stream.write(text.toByteArray(Charsets.UTF_8))
                                file.finishWrite(stream)
                            } catch (error: Exception) {
                                file.failWrite(stream)
                                throw error
                            }
                            null
                        }
                        Handler(Looper.getMainLooper()).post { result.success(value) }
                    } catch (error: Exception) {
                        Handler(Looper.getMainLooper()).post {
                            result.error("draft_storage", "Your local draft could not be saved or read.", null)
                        }
                    }
                }
            }

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "com.ziadg.badge_app/connection")
            .setMethodCallHandler { call, result ->
                try {
                    val token = call.argument<Number>("owner")?.toLong() ?: error("Missing connection owner")
                    when (call.method) {
                        "start" -> connectionLease.start(token) {
                            val intent = Intent(this, BadgeConnectionService::class.java)
                            if (Build.VERSION.SDK_INT >= 26) startForegroundService(intent) else startService(intent)
                        }
                        "stop" -> connectionLease.stop(token) { stopService(Intent(this, BadgeConnectionService::class.java)) }
                        else -> { result.notImplemented(); return@setMethodCallHandler }
                    }
                    result.success(null)
                } catch (_: Exception) { result.error("connection", "Background Bluetooth service could not start.", null) }
            }
        flutterEngine.dartExecutor.executeDartEntrypoint(DartExecutor.DartEntrypoint.createDefault())
        flutterEngine
    }
}
