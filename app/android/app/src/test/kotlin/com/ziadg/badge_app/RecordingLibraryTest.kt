package com.ziadg.badge_app

import org.junit.Assert.*
import org.junit.Test
import java.io.File
import java.nio.file.Files

class RecordingLibraryTest {
    private fun fixture(test: (File) -> Unit) {
        val dir = Files.createTempDirectory("badge-library").toFile()
        try { test(dir) } finally { dir.listFiles()?.forEach { it.delete() }; dir.delete() }
    }
    private fun recording(dir: File, name: String, size: Int) = File(dir, name).apply { writeBytes(ByteArray(size)) }
    @Test fun malformedEntriesDoNotHideValidLegacyAndCurrentFiles() = fixture { dir ->
        val old = recording(dir, "1000000000000-12345678.pba", 112)
        val current = recording(dir, "2000000000000-abcdef12.pba", 64+224)
        val orphan = recording(dir, "3000000000000-abcdef12.pba", 65)
        recording(dir, "4000000000000-abcdef12.pba", 0)
        recording(dir, "5000000000000-abcdef12.pba", 64)
        recording(dir, "invalid.pba", 112)
        recording(dir, "6000000000000-abcdef12.wav", 112)
        File(dir, "7000000000000-abcdef12.pba").mkdir()
        val entries = RecordingLibrary.list(dir)
        assertEquals(listOf(current.name, old.name), entries.map { it["id"] })
        assertEquals(listOf(0.02, 0.01), entries.map { it["seconds"] })
        assertEquals(65L, orphan.length()) // Listing neither repairs nor deletes data.
    }
    @Test fun activeFileIsExcludedAndLaterRecordingsRemainVisible() = fixture { dir ->
        val active = recording(dir, "2000000000000-abcdef12.pba", 64+112)
        val old = recording(dir, "1000000000000-12345678.pba", 112)
        recording(dir, "3000000000000-abcdef12.pba", 63)
        assertEquals(listOf(old.name), RecordingLibrary.list(dir, active).map { it["id"] })
        val later = recording(dir, "4000000000000-abcdef12.pba", 64+336)
        assertEquals(listOf(later.name, active.name, old.name), RecordingLibrary.list(dir).map { it["id"] })
    }
}
