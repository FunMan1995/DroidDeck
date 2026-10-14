package com.droiddeck.launcher.session

import org.junit.Assert.*
import org.junit.Test

class PerfModeTest {
    @Test fun oneTwentyHzFitsThirtyFortySixtyButNotNinety() {
        assertTrue(PerfMode.cadenceFits(120f, 30))
        assertTrue(PerfMode.cadenceFits(120f, 40))
        assertTrue(PerfMode.cadenceFits(120f, 60))
        assertFalse(PerfMode.cadenceFits(120f, 90))
        assertTrue(PerfMode.cadenceFits(120f, 0))
        assertTrue(PerfMode.cadenceFits(119.88f, 60))
    }

    @Test fun refreshHzIsCappedBySmoothDisplayOff() {
        assertEquals(60f, PerfMode.refreshHz(120f, 120f, 60f), 0.01f)
        assertEquals(60f, PerfMode.refreshHz(null, 120f, 60f), 0.01f)
        assertEquals(120f, PerfMode.refreshHz(120f, 60f, Float.POSITIVE_INFINITY), 0.01f)
        assertEquals(90f, PerfMode.refreshHz(null, 90f, Float.POSITIVE_INFINITY), 0.01f)
        assertEquals(60f, PerfMode.refreshHz(null, null, Float.POSITIVE_INFINITY), 0.01f)
        assertEquals(60f, PerfMode.refreshHz(0f, 0f, Float.POSITIVE_INFINITY), 0.01f)
    }

    @Test fun smoothDisplayOffOnlyWhenPeakCapIsBelowFastestMode() {
        assertTrue(PerfMode.smoothDisplayOff(60f, 120f))
        assertFalse(PerfMode.smoothDisplayOff(120f, 120f))
        assertFalse(PerfMode.smoothDisplayOff(Float.POSITIVE_INFINITY, 120f))
        assertFalse(PerfMode.smoothDisplayOff(60f, 60f))
    }
}
