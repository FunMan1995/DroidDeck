package com.droiddeck.launcher.session

import android.content.Context
import android.os.Looper
import android.os.PowerManager
import java.io.File
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [30], manifest = Config.NONE)
class ThermalComponentTest {
    @get:Rule val tmp = TemporaryFolder()
    private var component: ThermalComponent? = null
    private val limits = mutableListOf<Int>()
    private val frameGen = mutableListOf<Boolean>()
    private val logs = mutableListOf<String>()
    private val sink = object : ThermalComponent.Sink {
        override fun applyFpsLimit(limit: Int) { limits += limit; SessionState.fpsLimit = limit }
        override fun suppressFrameGen(suppressed: Boolean) { frameGen += suppressed }
        override fun log(message: String) { logs += message }
    }

    @After fun stop() {
        component?.stop(); SessionState.fpsLimit = 0
        shadowOf(power()).setCurrentThermalStatus(PowerManager.THERMAL_STATUS_NONE)
    }

    private fun power() = RuntimeEnvironment.getApplication().getSystemService(Context.POWER_SERVICE) as PowerManager

    @Test fun statusChangeWritesTheHudFileAndLowersTheCap() {
        val file = File(tmp.root, "run/droiddeck-hud/thermal")
        var now = 0L
        var headroom = 0.4f
        SessionState.fpsLimit = 0
        component = ThermalComponent(RuntimeEnvironment.getApplication(), file, ThermalPolicy(0, 120f), sink, { headroom }, { now })
            .also { it.start() }
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals("0 0.400", file.readText().trim())
        assertEquals(0, SessionState.thermalStatus)

        shadowOf(power()).setCurrentThermalStatus(PowerManager.THERMAL_STATUS_MODERATE)
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals("2 0.400", file.readText().trim())
        assertEquals(listOf(90), limits)
        assertEquals(90, SessionState.fpsLimit)
        assertTrue(logs.single().contains("fps cap lowered to 90"))

        now = 10_000; headroom = Float.NaN
        shadowOf(power()).setCurrentThermalStatus(PowerManager.THERMAL_STATUS_SEVERE)
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals("3 nan", file.readText().trim())
        assertEquals(listOf(true), frameGen)
        assertEquals(60, SessionState.fpsLimit)
    }

    @Test fun pollingRunsEveryTwoSecondsAndStopCleansUp() {
        val file = File(tmp.root, "thermal")
        var calls = 0
        component = ThermalComponent(RuntimeEnvironment.getApplication(), file, null, sink, { calls++; 0.2f }, { 0L })
            .also { it.start() }
        shadowOf(Looper.getMainLooper()).idle()
        val first = calls
        shadowOf(Looper.getMainLooper()).idleFor(java.time.Duration.ofMillis(ThermalComponent.POLL_MS * 3))
        assertEquals(first + 3, calls)
        assertTrue("guard off: no cap changes", limits.isEmpty())
        component!!.stop(); component = null
        assertFalse(file.exists())
        assertEquals(-1, SessionState.thermalStatus)
        assertTrue(SessionState.thermalHeadroom.isNaN())
    }

    @Test fun throwingHeadroomIsUnknown() {
        val file = File(tmp.root, "thermal")
        component = ThermalComponent(RuntimeEnvironment.getApplication(), file, ThermalPolicy(0, 120f), sink, { error("rate limited") }, { 0L })
            .also { it.start() }
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals("0 nan", file.readText().trim())
        assertTrue(limits.isEmpty())
    }
}
