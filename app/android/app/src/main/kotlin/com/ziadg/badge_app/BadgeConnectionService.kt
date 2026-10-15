package com.ziadg.badge_app

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder

/** Connected-device service only; recording never continues in the background. */
class BadgeConnectionService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val manager = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= 26) manager.createNotificationChannel(
            NotificationChannel("badge_connection", "Badge connection", NotificationManager.IMPORTANCE_LOW))
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        val builder = if (Build.VERSION.SDK_INT >= 26) Notification.Builder(this, "badge_connection") else Notification.Builder(this)
        val notification = builder.setSmallIcon(R.drawable.ic_connection)
            .setContentTitle("Pixel Badge connected")
            .setContentText("Tap to open controls")
            .setContentIntent(open).setOngoing(true).setShowWhen(false).build()
        if (Build.VERSION.SDK_INT >= 29) startForeground(41, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        else startForeground(41, notification)
        // Do not resurrect a dead BLE connection after process termination.
        return START_NOT_STICKY
    }
}
