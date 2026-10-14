package com.droiddeck.launcher.gpu

import com.droiddeck.launcher.gpu.GpuInfo.Family
import com.droiddeck.launcher.gpu.GpuInfo.Signals
import com.droiddeck.launcher.gpu.GpuInfo.Vendor
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class GpuVendorDetectionTest {
    private val komodo = Signals(socManufacturer = "Google", socModel = "Tensor G4", boardPlatform = "zumapro")

    @Test fun tensorG4FromSocSignals() {
        assertEquals(Vendor.MALI, GpuInfo.vendorOf(komodo))
        assertEquals("Tensor G4", GpuInfo.tensorOf(komodo)?.name)
        assertEquals("Mali-G715", GpuInfo.maliName(komodo))
        assertEquals(Vendor.MALI, GpuInfo.vendorOf(Signals(boardPlatform = "zumapro")))
        assertEquals(Vendor.MALI, GpuInfo.vendorOf(Signals(socManufacturer = "Google", socModel = "Tensor G4")))
    }

    @Test fun maliG715FromRendererAndVulkanVendorId() {
        // Pixel 9a profile, gpuinfo #50720: deviceName "Mali-G715", vendorID 5045 (0x13B5).
        assertEquals(Vendor.MALI, GpuInfo.vendorOf(Signals(renderer = "Mali-G715")))
        assertEquals(Vendor.MALI, GpuInfo.vendorOf(Signals(vulkanVendorId = 5045)))
        assertEquals(Vendor.MALI, GpuInfo.vendorOf(Signals(glVendor = "ARM")))
        assertEquals(0x13B5, GpuInfo.parseId("0x13b5"))
        assertEquals(5045, GpuInfo.parseId("5045"))
        assertEquals("Mali-G715", GpuInfo.maliName(Signals(renderer = "Mali-G715 MC7")))
    }

    @Test fun maliInfoIsRecognizedNotAdrenoAndNotTurnip() {
        val gpu = GpuInfo.maliInfo(komodo)
        assertEquals(Family.MALI, gpu.family)
        assertEquals(Vendor.MALI, gpu.vendor)
        assertEquals(GpuInfo.Support.UNTESTED, gpu.support)
        assertFalse(gpu.turnipCapable)
        assertEquals("Tensor G4", gpu.soc)
        assertNull(DriverPairs.recommendedKey(gpu, emptyList()))
    }

    @Test fun adrenoStaysAdreno() {
        assertEquals(Vendor.ADRENO, GpuInfo.vendorOf(Signals(renderer = "Adreno (TM) 750", glVendor = "Qualcomm")))
        assertEquals(Vendor.ADRENO, GpuInfo.vendorOf(Signals(vulkanVendorId = 0x5143)))
        assertEquals(Vendor.ADRENO, GpuInfo.vendorOf(Signals(socManufacturer = "QTI", socModel = "SM8650", boardPlatform = "pineapple")))
        val gpu = GpuInfo("Adreno 750", 750, Family.A7XX, "", false)
        assertEquals(Vendor.ADRENO, gpu.vendor)
        assertTrue(gpu.turnipCapable)
    }

    @Test fun unknownStaysUnknown() {
        assertEquals(Vendor.UNKNOWN, GpuInfo.vendorOf(Signals()))
        assertEquals(Vendor.UNKNOWN, GpuInfo.vendorOf(Signals(socManufacturer = "Google", socModel = "Tensor G5", boardPlatform = "laguna")))
        assertEquals(Vendor.UNKNOWN, GpuInfo.vendorOf(Signals(vulkanVendorId = 0x1010)))
        assertNull(GpuInfo.tensorOf(Signals(socModel = "zuma pro")))
        val gpu = GpuInfo("this GPU", 0, Family.NOT_ADRENO, "", false)
        assertEquals(Vendor.UNKNOWN, gpu.vendor)
        assertFalse(gpu.turnipCapable)
    }
}
