package com.droiddeck.launcher.session

import android.content.Context
import android.view.WindowManager

object SessionDisplay {
    const val MATCH_SCREEN = "screen"
    const val FOLLOW_SCREEN = "follow"
    const val FOLLOW_MIN_SIDE = 480

    const val DEFAULT_RESOLUTION = "720p"
    private val PRESET_HEIGHTS = listOf(720, 900, 1080)

    fun presetHeight(choice: String): Int? =
        if (choice.endsWith("p")) choice.dropLast(1).toIntOrNull()?.takeIf { it > 0 } else null

    fun screenSize(panel: Pair<Int, Int>): Pair<Int, Int> =
        (maxOf(panel.first, panel.second) and 1.inv()) to (minOf(panel.first, panel.second) and 1.inv())

    fun resolveChoice(panel: Pair<Int, Int>, choice: String): Pair<Int, Int> {
        if (choice == MATCH_SCREEN || choice == FOLLOW_SCREEN) return screenSize(panel)
        presetHeight(choice)?.let { return resolve(panel, it, SessionPrefs.SHAPE_AUTO) }
        val parts = choice.split('x').map { it.toIntOrNull() }
        return if (parts.size == 2 && parts.all { it != null && it > 0 }) parts[0]!! to parts[1]!!
        else resolve(panel, 720, SessionPrefs.SHAPE_AUTO)
    }

    fun resolutionOptions(panel: Pair<Int, Int>, modes: List<Pair<Int, Int>> = emptyList()): List<String> {
        val panelHeight = minOf(panel.first, panel.second)
        val heights = PRESET_HEIGHTS.filter { it <= panelHeight }.map { "${it}p" }
            .filter { resolveChoice(panel, it) != screenSize(panel) }
        val taken = heights.map { resolveChoice(panel, it) }.toSet()
        return heights + presets(panel, modes).filter { resolveChoice(panel, it) !in taken } + MATCH_SCREEN + FOLLOW_SCREEN
    }

    /** Smallest short side for the "panel ÷2" preset. */
    const val HALF_MIN_SIDE = 540

    /**
     * Panel-derived presets (brief 005 §1), as "WxH" choices in landscape order:
     *  - "panel ÷2": an exact half of the panel for integer scaling (2992×1344 → 1496×672);
     *  - a 1080-class reduced size: a smaller same-aspect display mode when [modes] lists one (the
     *    Pixel "default resolution" set, e.g. 2244×1008), else 3/4 of the panel when that is 1080-class.
     * Portrait panels and modes are swapped to landscape first.
     */
    fun presets(panel: Pair<Int, Int>, modes: List<Pair<Int, Int>>): List<String> {
        val (pw, ph) = maxOf(panel.first, panel.second) to minOf(panel.first, panel.second)
        if (pw <= 0 || ph <= 0) return emptyList()
        val out = LinkedHashSet<Pair<Int, Int>>()
        if (pw % 2 == 0 && ph % 2 == 0 && ph / 2 >= HALF_MIN_SIDE) out += (pw / 2) to (ph / 2)
        val aspect = pw.toFloat() / ph
        val reduced = modes.asSequence()
            .map { maxOf(it.first, it.second) to minOf(it.first, it.second) }
            .filter { (w, h) -> h in 900 until ph && w < pw && Math.abs(w.toFloat() / h - aspect) < 0.01f }
            .maxByOrNull { it.second }
            ?: (((pw * 3 / 4) and 1.inv()) to ((ph * 3 / 4) and 1.inv())).takeIf { it.second in 900..1100 }
        if (reduced != null) out += (reduced.first and 1.inv()) to (reduced.second and 1.inv())
        return out.filter { it != screenSize(panel) }.map { "${it.first}x${it.second}" }
    }

    fun presetsForModes(panel: Pair<Int, Int>, modes: List<android.util.Size>): List<String> =
        presets(panel, modes.map { it.width to it.height })

    fun supportedModeSizes(context: Context): List<Pair<Int, Int>> = try {
        val manager = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
        @Suppress("DEPRECATION")
        manager.defaultDisplay.supportedModes.map { it.physicalWidth to it.physicalHeight }.distinct()
    } catch (_: Throwable) { emptyList() }

    fun followSize(width: Int, height: Int): Pair<Int, Int>? =
        if (minOf(width, height) < FOLLOW_MIN_SIDE) null else (width and 1.inv()) to (height and 1.inv())

    const val UNFOLDED_MAX_GAME_SHARE = 0.8f

    fun foldable(context: Context): Boolean =
        context.packageManager.hasSystemFeature("android.hardware.sensor.hinge_angle")

    const val UNFOLDED_MIN_DP = 600

    fun unfoldedGameHeight(width: Int, height: Int): Int =
        minOf(width * 9 / 16, (height * UNFOLDED_MAX_GAME_SHARE).toInt()) and 1.inv()


    fun panelSize(context: Context): Pair<Int, Int> {
        val manager = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
        @Suppress("DEPRECATION")
        val mode = manager.defaultDisplay.mode
        return maxOf(mode.physicalWidth, mode.physicalHeight) to minOf(mode.physicalWidth, mode.physicalHeight)
    }

    fun resolve(panel: Pair<Int, Int>, cap: Int, shape: String, custom: Pair<Int, Int>? = null): Pair<Int, Int> {
        custom?.let { return it }
        val panelW = maxOf(panel.first, panel.second).toFloat()
        val panelH = minOf(panel.first, panel.second).toFloat().coerceAtLeast(1f)
        val aspect = when (shape) {
            SessionPrefs.SHAPE_WIDE -> 16f / 9f
            SessionPrefs.SHAPE_EXACT -> panelW / panelH
            else -> maxOf(panelW / panelH, 16f / 9f)
        }
        val height = (if (cap <= 0) panelH else minOf(panelH, cap.toFloat())).toInt()
        return ((height * aspect).toInt() and 1.inv()) to (height and 1.inv())
    }

    fun canStretch16x9(size: Pair<Int, Int>): Boolean =
        (size.second * 16 + 4) / 9 / 2 * 2 > size.first
}
