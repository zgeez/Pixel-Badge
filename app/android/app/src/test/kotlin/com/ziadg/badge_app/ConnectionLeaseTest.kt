package com.ziadg.badge_app

import org.junit.Assert.*
import org.junit.Test

class ConnectionLeaseTest {
    @Test fun oldReleaseCannotStopReplacementConnection() {
        val lease = ConnectionLease()
        var running = false
        lease.start(1) { running = true }
        lease.start(2) { running = true }
        lease.stop(1) { running = false }
        assertTrue(running)
        lease.stop(2) { running = false }
        assertFalse(running)
        lease.stop(2) { fail("Released a lease twice") }
    }
    @Test fun failedReplacementRetainsPreviousOwner() {
        val lease = ConnectionLease()
        lease.start(1) {}
        try { lease.start(2) { error("Platform refused start") }; fail("Failure lost") }
        catch (_: IllegalStateException) {}
        var stopped = false
        lease.stop(2) { fail("Failed start acquired ownership") }
        lease.stop(1) { stopped = true }
        assertTrue(stopped)
    }
}
