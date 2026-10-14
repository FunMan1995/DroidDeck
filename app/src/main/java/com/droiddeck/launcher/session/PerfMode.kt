package com.droiddeck.launcher.session

import android.app.Activity
import android.app.GameManager
import android.app.GameState
import android.content.Context
import android.os.Build
import android.provider.Settings
import android.util.Log

object PerfMode {
    private const val TAG = "PerfMode"

    fun apply(a: Activity, fpsLimit: Int = 0): String {
        val parts = ArrayList<String>(2)

        parts += try {
            val display = if (Build.VERSION.SDK_INT >= 30) a.display else @Suppress("DEPRECATION") a.windowManager.defaultDisplay
            if (display == null || Build.VERSION.SDK_INT < 23) "display mode unchanged" else {
                val cur = display.mode
                val cap = peakRefreshCap(a)
                val sameSize = display.supportedModes
                    .filter { it.physicalWidth == cur.physicalWidth && it.physicalHeight == cur.physicalHeight }
                    .filter { it.refreshRate <= cap + 0.5f }
                    .ifEmpty { display.supportedModes.filter { it.modeId == cur.modeId } }
                val best = sameSize.filter { cadenceFits(it.refreshRate, fpsLimit) }.maxByOrNull { it.refreshRate }
                    ?: sameSize.maxByOrNull { it.refreshRate }
                if (best != null && best.modeId != cur.modeId &&
                    (best.refreshRate > cur.refreshRate + 0.5f || !cadenceFits(cur.refreshRate, fpsLimit))) {
                    a.window.attributes = a.window.attributes.apply { preferredDisplayModeId = best.modeId }
                    "display ${cur.refreshRate.toInt()} → ${best.refreshRate.toInt()} Hz (mode ${best.modeId})"
                } else "display ${cur.refreshRate.toInt()} Hz"
            }
        } catch (t: Throwable) { Log.w(TAG, "display mode", t); "display mode failed" }

        parts += if (Build.VERSION.SDK_INT >= 31) {
            val gm = a.getSystemService(GameManager::class.java)
            if (gm == null) "no GameManager" else {
                val mode = when (gm.gameMode) {
                    GameManager.GAME_MODE_PERFORMANCE -> "performance"
                    GameManager.GAME_MODE_BATTERY -> "battery"
                    GameManager.GAME_MODE_STANDARD -> "standard"
                    else -> "unsupported"
                }
                if (Build.VERSION.SDK_INT >= 33) {
                    try { gm.setGameState(GameState(false, GameState.MODE_GAMEPLAY_INTERRUPTIBLE)) } catch (t: Throwable) { Log.w(TAG, "game state", t) }
                }
                "game mode $mode"
            }
        } else "game mode needs Android 12"

        smoothDisplayNote(a)?.let { parts += it }
        return parts.joinToString(" · ")
    }

    fun peakRefreshCap(context: Context): Float {
        val peak = try {
            Settings.System.getFloat(context.contentResolver, "peak_refresh_rate")
        } catch (_: Throwable) { 0f }
        return if (peak >= 1f) peak else Float.POSITIVE_INFINITY
    }

    /** Output refresh for the guest: the preferred mode's rate, else the display's, capped by "Smooth display". */
    fun refreshHz(preferredModeHz: Float?, displayHz: Float?, peakCap: Float): Float {
        val capped = minOf(preferredModeHz ?: displayHz ?: 60f, peakCap)
        return if (capped > 1f) capped else 60f
    }

    /** True when the system "Smooth display" setting caps the panel below its fastest mode (shown in HUD/perf dialog). */
    fun smoothDisplayOff(peakCap: Float, fastestModeHz: Float): Boolean =
        peakCap.isFinite() && peakCap + 0.5f < fastestModeHz

    /** Localised "Smooth display is off: capped at N Hz", or null when the setting doesn't limit the panel. */
    fun smoothDisplayNote(context: Context): String? = try {
        val cap = peakRefreshCap(context)
        @Suppress("DEPRECATION")
        val display = (context.getSystemService(Context.WINDOW_SERVICE) as android.view.WindowManager).defaultDisplay
        val fastest = display.supportedModes.maxOfOrNull { it.refreshRate } ?: 0f
        if (smoothDisplayOff(cap, fastest)) context.getString(com.droiddeck.launcher.R.string.perf_smooth_display_off, Math.round(cap)) else null
    } catch (t: Throwable) { Log.w(TAG, "smooth display", t); null }

    fun cadenceFits(hz: Float, fpsLimit: Int): Boolean {
        if (fpsLimit <= 0) return true
        val ratio = hz / fpsLimit
        return ratio >= 0.98f && Math.abs(ratio - Math.round(ratio)) < 0.02f
    }
}
