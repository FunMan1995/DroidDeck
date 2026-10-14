package com.droiddeck.launcher.core

import com.droiddeck.launcher.gpu.GpuInfo
import com.droiddeck.launcher.gpu.GpuInfo.Vendor
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class DeviceSupportVendorTest {
    private val komodo = GpuInfo.Signals(socManufacturer = "Google", socModel = "Tensor G4", boardPlatform = "zumapro")

    @Test fun tensorG4IsMaliAndRecognized() {
        val v = DeviceSupport.vendorFor(false, GpuInfo.vendorOf(komodo))
        assertEquals(Vendor.MALI, v)
        assertTrue(DeviceSupport.recognized(v))
    }

    @Test fun adrenoNodeWins() {
        assertEquals(Vendor.ADRENO, DeviceSupport.vendorFor(true, Vendor.UNKNOWN))
        assertTrue(DeviceSupport.recognized(Vendor.ADRENO))
    }

    @Test fun unknownIsNotRecognized() {
        val v = DeviceSupport.vendorFor(false, GpuInfo.vendorOf(GpuInfo.Signals()))
        assertEquals(Vendor.UNKNOWN, v)
        assertFalse(DeviceSupport.recognized(v))
    }
}
