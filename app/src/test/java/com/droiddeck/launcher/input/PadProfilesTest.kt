package com.droiddeck.launcher.input

import org.junit.Assert.*
import org.junit.Test

class PadProfilesTest {
    @Test fun knownPadsMapToTheirIdentityAndEverythingElseStaysXbox360() {
        assertEquals("ps5", PadProfiles.identityFor(0x054c, 0x0ce6))
        assertEquals("ps5", PadProfiles.identityFor(0x054c, 0x0df2))
        assertEquals("ps4", PadProfiles.identityFor(0x054c, 0x09cc))
        assertEquals("xboxone", PadProfiles.identityFor(0x045e, 0x0b13))
        assertEquals("xbox360", PadProfiles.identityFor(0x045e, 0x028e))
        assertEquals("xbox360", PadProfiles.identityFor(0x2dc8, 0x3106)) // 8BitDo
        assertEquals("xbox360", PadProfiles.identityFor(0x358a, 0x0104)) // Backbone
        assertEquals("xbox360", PadProfiles.identityFor(0, 0))
        assertTrue(PadProfiles.isDualSense(0x054c, 0x0ce6))
        assertFalse(PadProfiles.isDualSense(0x054c, 0x09cc))
    }

    @Test fun presentedIdsMatchTheRealKernelDevices() {
        assertEquals(PadProfiles.Presented(0x054c, 0x0ce6, "Sony Interactive Entertainment DualSense Wireless Controller"),
            PadProfiles.presented("ps5"))
        assertEquals(0x028e, PadProfiles.presented("xbox360").product)
        assertEquals(0x028e, PadProfiles.presented("anything else").product)
    }

    @Test fun sonyGenericLayoutOnlyWithoutTriggerAxes() {
        assertTrue(PadProfiles.sonyGenericLayout(0x054c, true, true, false, false))
        assertFalse(PadProfiles.sonyGenericLayout(0x054c, true, true, true, false))
        assertFalse(PadProfiles.sonyGenericLayout(0x054c, true, true, false, true))
        assertFalse(PadProfiles.sonyGenericLayout(0x045e, true, true, false, false))
        assertFalse(PadProfiles.sonyGenericLayout(0x054c, false, false, false, false))
    }

    @Test fun triggersRescaleFromSignedRanges() {
        assertEquals(0f, PadProfiles.trigger(-1f, -1f), 1e-6f)
        assertEquals(0.5f, PadProfiles.trigger(0f, -1f), 1e-6f)
        assertEquals(1f, PadProfiles.trigger(1f, -1f), 1e-6f)
        assertEquals(0.25f, PadProfiles.trigger(0.25f, 0f), 1e-6f)
        assertEquals(1f, PadProfiles.trigger(1.2f, 0f), 1e-6f)
    }
}
