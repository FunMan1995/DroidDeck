package com.droiddeck.launcher.session

import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.os.SystemClock
import android.util.Log
import com.droiddeck.launcher.core.SessionPart
import java.io.File

/**
 * Session thermal state (brief 004 §1-2). Listens to PowerManager thermal status (API 29+), polls
 * getThermalHeadroom(10) every [POLL_MS] (API 30+; the platform rate-limits it to about once a second
 * and returns NaN then, which is treated as unknown), writes "status headroom" to [file] in the rootfs
 * (run/droiddeck-hud/thermal) for the HUD and the guest, mirrors both into [SessionState], and,
 * when the Thermal guard is on, applies [ThermalPolicy] through [sink].
 */
class ThermalComponent(
    private val context: Context,
    private val file: File,
    private val guard: ThermalPolicy?,
    private val sink: Sink,
    private val headroom: () -> Float = defaultHeadroom(context),
    private val clock: () -> Long = { SystemClock.elapsedRealtime() },
) : SessionPart() {
    interface Sink {
        fun applyFpsLimit(limit: Int)
        fun suppressFrameGen(suppressed: Boolean)
        fun log(message: String)
    }

    private val power = context.getSystemService(Context.POWER_SERVICE) as PowerManager
    private val main = Handler(Looper.getMainLooper())
    private var listener: Any? = null
    @Volatile private var running = false
    @Volatile private var status = 0
    private var thread: Thread? = null

    private val poll = object : Runnable {
        override fun run() {
            if (!running) return
            sample()
            main.postDelayed(this, POLL_MS)
        }
    }

    override fun start() {
        running = true
        status = currentStatus()
        if (Build.VERSION.SDK_INT >= 29) {
            val l = PowerManager.OnThermalStatusChangedListener { s -> status = s; if (running) sample() }
            runCatching { power.addThermalStatusListener(l); listener = l }
                .onFailure { Log.w(TAG, "thermal: status listener unavailable: $it") }
        }
        val support = if (Build.VERSION.SDK_INT >= 30) "headroom forecast" else "status only (headroom needs API 30)"
        Log.i(TAG, "thermal: watching ($support), guard ${if (guard != null) "on" else "off"}")
        main.post(poll)
    }

    override fun stop() {
        running = false
        main.removeCallbacks(poll)
        if (Build.VERSION.SDK_INT >= 29) (listener as? PowerManager.OnThermalStatusChangedListener)?.let {
            runCatching { power.removeThermalStatusListener(it) }
        }
        listener = null
        file.delete()
        SessionState.thermalStatus = -1
        SessionState.thermalHeadroom = Float.NaN
        SessionState.thermalFrameGenSuppressed = false
    }

    /** One evaluation; public for tests. Main thread. */
    fun sample() {
        val h = runCatching { headroom() }.getOrDefault(Float.NaN).let { if (it.isInfinite()) Float.NaN else it }
        val s = status
        SessionState.thermalStatus = s
        SessionState.thermalHeadroom = h
        write(s, h)
        val g = guard ?: return
        val before = g.fpsLimit
        val fgBefore = g.frameGenSuppressed
        val d = g.update(s, h, clock())
        if (d.fpsLimit != before) sink.applyFpsLimit(d.fpsLimit)
        if (d.frameGenSuppressed != fgBefore) sink.suppressFrameGen(d.frameGenSuppressed)
        d.reason?.let { sink.log("thermal guard: $it") }
    }

    private fun currentStatus(): Int =
        if (Build.VERSION.SDK_INT >= 29) runCatching { power.currentThermalStatus }.getOrDefault(0) else 0

    private fun write(status: Int, headroom: Float) {
        try {
            file.parentFile?.mkdirs()
            val tmp = File(file.parentFile, file.name + ".tmp")
            tmp.writeText("$status ${if (headroom.isNaN()) "nan" else String.format(java.util.Locale.ROOT, "%.3f", headroom)}\n")
            if (!tmp.renameTo(file)) tmp.delete()
        } catch (e: Exception) {
        }
    }

    companion object {
        private const val TAG = "SessionService"
        const val HUD_FILE = "run/droiddeck-hud/thermal"
        const val POLL_MS = 2_000L
        const val FORECAST_SECONDS = 10

        fun defaultHeadroom(context: Context): () -> Float {
            val power = context.getSystemService(Context.POWER_SERVICE) as PowerManager
            return { if (Build.VERSION.SDK_INT >= 30) power.getThermalHeadroom(FORECAST_SECONDS) else Float.NaN }
        }
    }
}
