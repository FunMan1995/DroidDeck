package com.droiddeck.launcher.session

import com.droiddeck.launcher.gpu.LinuxVulkanDriverManager
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class VortekSelectionTest {
    @Test fun maliWithPrefOffGetsNoVortekEnv() {
        val id = VortekSelection.driverId("", mali = true, forwarderPref = false)
        assertEquals("", id)
        assertTrue(VortekSelection.guestEnv(id).isEmpty())
    }

    @Test fun maliWithPrefOnUsesVortek() {
        val id = VortekSelection.driverId("", mali = true, forwarderPref = true)
        assertEquals(LinuxVulkanDriverManager.VORTEK_ID, id)
        assertEquals(listOf("VORTEK_SERVER_PATH=/run/droiddeck/vortek/V0"), VortekSelection.guestEnv(id))
    }

    @Test fun nonMaliNeverUsesVortek() {
        assertEquals("", VortekSelection.driverId("", mali = false, forwarderPref = true))
    }

    @Test fun explicitDriverWins() {
        val id = VortekSelection.driverId("turnip26", mali = true, forwarderPref = true)
        assertEquals("turnip26", id)
        assertTrue(VortekSelection.guestEnv(id).isEmpty())
    }
}
