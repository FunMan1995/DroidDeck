package com.droiddeck.launcher.session

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ThermalPolicyTest {
    private val nan = Float.NaN

    @Test fun coolSessionKeepsTheUserLimit() {
        val p = ThermalPolicy(0, 120f)
        val d = p.update(0, 0.3f, 0)
        assertEquals(0, d.fpsLimit)
        assertFalse(d.frameGenSuppressed)
        assertNull(d.reason)
    }

    @Test fun hotStepsDownOneStepPerIntervalThroughTheTable() {
        val p = ThermalPolicy(0, 120f)
        assertEquals(90, p.update(0, 0.9f, 0).fpsLimit)
        assertEquals("rate limited between steps", 90, p.update(0, 0.9f, 5_000).fpsLimit)
        assertEquals(60, p.update(0, 0.9f, 10_000).fpsLimit)
        assertEquals(40, p.update(0, 0.9f, 20_000).fpsLimit)
        assertEquals(30, p.update(0, 0.9f, 30_000).fpsLimit)
        assertEquals("floor", 30, p.update(0, 0.9f, 40_000).fpsLimit)
    }

    @Test fun moderateStatusIsHotEvenWithUnknownHeadroom() {
        val p = ThermalPolicy(0, 120f)
        val d = p.update(ThermalPolicy.STATUS_MODERATE, nan, 0)
        assertEquals(90, d.fpsLimit)
        assertTrue(d.reason!!.contains("MODERATE"))
    }

    @Test fun nanHeadroomNeitherTightensNorRestores() {
        val p = ThermalPolicy(0, 120f)
        p.update(0, 0.9f, 0)
        for (t in 1..200) assertEquals(90, p.update(0, nan, t * 1_000L).fpsLimit)
    }

    @Test fun hysteresisBandHolds() {
        val p = ThermalPolicy(0, 120f)
        p.update(0, 0.9f, 0)
        for (t in 1..200) assertEquals(90, p.update(1, 0.75f, t * 1_000L).fpsLimit)
    }

    @Test fun restoresOneStepAfterSixtySecondsCoolAndTimerRestarts() {
        val p = ThermalPolicy(0, 120f)
        p.update(0, 0.9f, 0); p.update(0, 0.9f, 10_000) // 60
        assertEquals(60, p.update(0, 0.5f, 20_000).fpsLimit) // cool from 20 s
        assertEquals(60, p.update(0, 0.5f, 79_999).fpsLimit)
        assertEquals(90, p.update(0, 0.5f, 80_000).fpsLimit)
        assertEquals(90, p.update(0, 0.5f, 139_999).fpsLimit)
        val back = p.update(0, 0.5f, 140_000)
        assertEquals("back to the user's limit (uncapped)", 0, back.fpsLimit)
        assertTrue(back.reason!!.contains("uncapped"))
        assertFalse(p.engaged)
    }

    @Test fun aWarmSampleResetsTheCoolTimer() {
        val p = ThermalPolicy(0, 120f)
        p.update(0, 0.9f, 0)
        p.update(0, 0.5f, 1_000)
        p.update(0, 0.75f, 50_000) // band: resets
        assertEquals(90, p.update(0, 0.5f, 61_000).fpsLimit)
        assertEquals(90, p.update(0, 0.5f, 110_000).fpsLimit)
        assertEquals(0, p.update(0, 0.5f, 121_000).fpsLimit)
    }

    @Test fun neverRaisesAboveTheUserLimit() {
        val p = ThermalPolicy(45, 120f)
        assertEquals(40, p.update(0, 0.9f, 0).fpsLimit)
        assertEquals(30, p.update(0, 0.9f, 10_000).fpsLimit)
        assertEquals(40, p.update(0, 0.1f, 20_000).let { p.update(0, 0.1f, 80_000) }.fpsLimit)
        assertEquals(45, p.update(0, 0.1f, 140_000).fpsLimit)
    }

    @Test fun uncappedSixtyHertzStartsBelowRefresh() {
        val p = ThermalPolicy(0, 60f)
        assertEquals(40, p.update(0, 0.9f, 0).fpsLimit)
    }

    @Test fun severeSuppressesFrameGenUntilBelowSevere() {
        val p = ThermalPolicy(0, 120f)
        val d = p.update(ThermalPolicy.STATUS_SEVERE, 1.0f, 0)
        assertTrue(d.frameGenSuppressed)
        assertEquals(90, d.fpsLimit)
        assertTrue(p.update(4, 1.1f, 1_000).frameGenSuppressed)
        val lifted = p.update(ThermalPolicy.STATUS_MODERATE, 0.9f, 2_000)
        assertFalse(lifted.frameGenSuppressed)
        assertTrue(lifted.reason!!.contains("allowed again"))
    }

    @Test fun guardDefaultsOnOnlyForKomodo() {
        assertTrue(SessionPrefs.thermalGuardDefault("komodo"))
        assertFalse(SessionPrefs.thermalGuardDefault("caiman"))
        assertFalse(SessionPrefs.thermalGuardDefault(null))
    }
}
