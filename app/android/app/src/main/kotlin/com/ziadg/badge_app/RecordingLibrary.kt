package com.ziadg.badge_app

import java.io.File

/** Listing must not mutate capture/session state or let one damaged file hide others. */
internal object RecordingLibrary {
    private val filename = Regex("[0-9]+-[a-f0-9]{8}\\.pba")
    fun validId(id: String) = filename.matches(id)
    fun list(directory: File, active: File? = null): List<Map<String, Any>> =
        directory.listFiles()?.sortedByDescending { it.name }?.mapNotNull { entry ->
            try {
                if (!entry.isFile || entry == active || !validId(entry.name)) return@mapNotNull null
                val blocks = AudioStorage.blockCount(entry.length())
                if (blocks == 0L) null else mapOf("id" to entry.name, "seconds" to blocks/100.0)
            } catch (_: IllegalArgumentException) { null }
              catch (_: IllegalStateException) { null }
              catch (_: SecurityException) { null }
        } ?: emptyList()
}
