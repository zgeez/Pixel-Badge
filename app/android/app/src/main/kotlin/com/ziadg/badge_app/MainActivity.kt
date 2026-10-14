package com.ziadg.badge_app

import android.content.pm.ApplicationInfo
import android.os.Bundle
import android.view.WindowManager
import android.os.Handler
import android.os.Looper
import android.util.AtomicFile
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import java.io.File
import java.io.FileNotFoundException
import java.util.concurrent.Executors

class MainActivity : FlutterActivity() {
    private val draftIo = Executors.newSingleThreadExecutor()
    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
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
                                require(file.baseFile.length() <= 65536)
                                it.readText()
                            }
                        } else {
                            val text = call.arguments as? String ?: error("Invalid draft")
                            require(text.toByteArray(Charsets.UTF_8).size <= 65536)
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
    }
    override fun onDestroy() {
        draftIo.shutdown()
        super.onDestroy()
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if ((applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0) {
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }
    }
}
