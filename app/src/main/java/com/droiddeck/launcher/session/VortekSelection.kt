package com.droiddeck.launcher.session

import com.droiddeck.launcher.gpu.LinuxVulkanDriverManager
import com.droiddeck.launcher.runtime.LinuxRuntime

/** Pure selection logic for the Vortek forwarder (brief 003), kept out of SessionService for tests. */
object VortekSelection {
    const val SOCKET_SUBDIR = "vortek"
    const val SOCKET_NAME = "V0"
    val GUEST_SOCKET: String = LinuxRuntime.GUEST_RUNTIME_DIR + "/" + SOCKET_SUBDIR + "/" + SOCKET_NAME

    /**
     * An explicit Linux driver choice always wins. With none, a Mali device uses Vortek only when the
     * opt-in forwarder pref is on.
     */
    fun driverId(chosen: String, mali: Boolean, forwarderPref: Boolean): String =
        if (chosen.isEmpty() && mali && forwarderPref) LinuxVulkanDriverManager.VORTEK_ID else chosen

    fun guestEnv(driverId: String): List<String> =
        if (driverId == LinuxVulkanDriverManager.VORTEK_ID) listOf("VORTEK_SERVER_PATH=$GUEST_SOCKET") else emptyList()
}
