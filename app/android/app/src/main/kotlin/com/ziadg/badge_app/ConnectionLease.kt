package com.ziadg.badge_app

/** A stale link may release only its own connected-device service lease. */
internal class ConnectionLease {
    private var owner: Long? = null
    fun start(token: Long, action: () -> Unit) {
        require(token > 0)
        action()
        owner = token
    }
    fun stop(token: Long, action: () -> Unit) {
        if (owner == token) {
            action()
            owner = null
        }
    }
}
