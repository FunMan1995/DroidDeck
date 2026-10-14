package com.droiddeck.launcher.wayland

import org.junit.Assert.*
import org.junit.Test

class HdrSupportTest {
    @Test fun sdrRatioNeedsApi34AndAvailability() {
        assertEquals(true to 2.5f, HdrSupport.sdrRatio(34, { true }, { 2.5f }))
        assertEquals(false to -1f, HdrSupport.sdrRatio(33, { true }, { 2.5f }))
        assertEquals(false to -1f, HdrSupport.sdrRatio(35, { false }, { 2.5f }))
        assertEquals(false to -1f, HdrSupport.sdrRatio(34, { throw IllegalStateException() }, { 2.5f }))
        assertEquals(true to -1f, HdrSupport.sdrRatio(34, { true }, { throw IllegalStateException() }))
    }

    @Test fun probeLogTagsKomodoAndShowsTheRatioPath() {
        val probe = HdrProbe(true, "HDR10, HLG", 3000f, 1000f, 0.0005f, true, 1.75f, 0, "Built-in Screen", null)
        val line = HdrSupport.logLine(probe, "komodo", "Pixel 9 Pro XL", false)
        assertTrue(line, line.startsWith("hdr probe on komodo (Pixel 9 Pro XL): display 0"))
        assertTrue(line, "sdrRatio available (1.75)" in line && line.endsWith("requested=false"))
        val off = HdrSupport.logLine(HdrProbe(false, "", 0f, 0f, 0f, false, -1f, 0, "d", "no HDR"), "husky", "Pixel 8 Pro", true)
        assertTrue(off, off.startsWith("hdr probe on husky (Pixel 8 Pro)") && "sdrRatio unavailable;" in off && off.endsWith("(off: no HDR)"))
    }
}
