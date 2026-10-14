package com.droiddeck.launcher.session

/**
 * Thermal guard policy (brief 004 §2), pure so it can be unit tested.
 *
 * Inputs: [PowerManager] thermal status (NONE=0 ... SHUTDOWN=6), thermal headroom (NaN = unknown,
 * e.g. rate-limited or unsupported) and a monotonic clock in ms.
 * - Hot (headroom >= [HOT_HEADROOM] or status >= MODERATE): lower the FPS cap one step
 *   (120 -> 90 -> 60 -> 40 -> 30), at most once per [STEP_DOWN_INTERVAL_MS].
 * - SEVERE or worse: frame generation is also suppressed until the status drops below SEVERE.
 * - Cool (headroom known and < [COOL_HEADROOM], status < MODERATE) for [RESTORE_AFTER_MS]:
 *   restore one step; the timer restarts after each restore and whenever it is not cool.
 * - In between, or with unknown headroom and status < MODERATE: hold (hysteresis). NaN never
 *   counts as cool or hot, so a rate-limited read cannot release or tighten the cap.
 * The guard never raises the cap above the user's own limit ([userLimit], 0 = uncapped at refresh).
 */
class ThermalPolicy(private val userLimit: Int, refreshHz: Float) {
    data class Decision(val fpsLimit: Int, val frameGenSuppressed: Boolean, val reason: String?)

    private val ceiling: Int = if (userLimit > 0) userLimit else refreshHz.toInt().coerceAtLeast(STEPS.last())
    private var cap = ceiling
    private var suppressFrameGen = false
    private var lastStepDownMs = Long.MIN_VALUE
    private var coolSinceMs = -1L

    /** Effective limit for SessionState.fpsLimit: the user's own limit while the guard is idle. */
    val fpsLimit: Int get() = if (cap >= ceiling) userLimit else cap
    val frameGenSuppressed: Boolean get() = suppressFrameGen
    val engaged: Boolean get() = cap < ceiling || suppressFrameGen

    fun update(status: Int, headroom: Float, nowMs: Long): Decision {
        val known = !headroom.isNaN()
        val hot = (known && headroom >= HOT_HEADROOM) || status >= STATUS_MODERATE
        val cool = known && headroom < COOL_HEADROOM && status < STATUS_MODERATE
        var reason: String? = null

        val severe = status >= STATUS_SEVERE
        if (severe != suppressFrameGen) {
            suppressFrameGen = severe
            reason = if (severe) "status ${statusName(status)}: frame generation off" else "status ${statusName(status)}: frame generation allowed again"
        }

        if (hot) {
            coolSinceMs = -1L
            val lower = nextLower(cap)
            if (lower != null && (lastStepDownMs == Long.MIN_VALUE || nowMs - lastStepDownMs >= STEP_DOWN_INTERVAL_MS)) {
                cap = lower
                lastStepDownMs = nowMs
                reason = join(reason, "${why(status, headroom)}: fps cap lowered to $cap")
            }
        } else if (cool && cap < ceiling) {
            if (coolSinceMs < 0) coolSinceMs = nowMs
            if (nowMs - coolSinceMs >= RESTORE_AFTER_MS) {
                cap = nextHigher(cap)
                coolSinceMs = nowMs
                reason = join(reason, "cool for ${RESTORE_AFTER_MS / 1000} s (headroom ${fmt(headroom)}): fps cap " +
                    if (cap >= ceiling) "restored to ${if (userLimit > 0) "$userLimit" else "uncapped"}" else "raised to $cap")
            }
        } else {
            coolSinceMs = -1L
        }
        return Decision(fpsLimit, suppressFrameGen, reason)
    }

    private fun nextLower(current: Int): Int? = STEPS.firstOrNull { it < current }

    private fun nextHigher(current: Int): Int = STEPS.lastOrNull { it > current && it < ceiling } ?: ceiling

    private fun why(status: Int, headroom: Float) =
        if (status >= STATUS_MODERATE) "status ${statusName(status)}" else "headroom ${fmt(headroom)}"

    private fun join(a: String?, b: String) = if (a == null) b else "$a; $b"

    companion object {
        val STEPS = listOf(120, 90, 60, 40, 30)
        const val HOT_HEADROOM = 0.85f
        const val COOL_HEADROOM = 0.7f
        const val RESTORE_AFTER_MS = 60_000L
        const val STEP_DOWN_INTERVAL_MS = 10_000L
        const val STATUS_MODERATE = 2
        const val STATUS_SEVERE = 3

        fun fmt(headroom: Float): String = if (headroom.isNaN()) "unknown" else String.format(java.util.Locale.ROOT, "%.2f", headroom)

        fun statusName(status: Int): String = when (status) {
            0 -> "NONE"; 1 -> "LIGHT"; 2 -> "MODERATE"; 3 -> "SEVERE"; 4 -> "CRITICAL"; 5 -> "EMERGENCY"; 6 -> "SHUTDOWN"
            else -> "UNKNOWN($status)"
        }
    }
}
