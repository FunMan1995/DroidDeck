package com.droiddeck.launcher.input

import kotlin.math.abs

/**
 * Brief 005 §4: keep touch controls out from under the camera hole. Holes are display-cutout
 * bounding rects as [left, top, right, bottom] in view pixels (plain ints so this stays testable).
 */
object CutoutAvoid {
    fun holes(insets: android.view.WindowInsets?): List<IntArray> =
        if (android.os.Build.VERSION.SDK_INT >= 28)
            insets?.displayCutout?.boundingRects?.map { intArrayOf(it.left, it.top, it.right, it.bottom) } ?: emptyList()
        else emptyList()

    /** Moves a control of half-size [rx]×[ry] centred at ([x],[y]) the shortest distance off every hole, staying on screen. */
    fun nudge(x: Float, y: Float, rx: Float, ry: Float, holes: List<IntArray>, w: Int, h: Int, margin: Float = 0f): Pair<Float, Float> {
        var cx = x; var cy = y
        for (hole in holes) {
            val l = hole[0] - margin; val t = hole[1] - margin; val r = hole[2] + margin; val b = hole[3] + margin
            if (cx + rx <= l || cx - rx >= r || cy + ry <= t || cy - ry >= b) continue
            val moves = listOf(
                cx to b + ry, cx to t - ry,   // below / above
                r + rx to cy, l - rx to cy,   // right / left
            ).filter { (mx, my) -> mx - rx >= 0f && mx + rx <= w && my - ry >= 0f && my + ry <= h }
            val best = moves.minByOrNull { (mx, my) -> abs(mx - cx) + abs(my - cy) } ?: continue
            cx = best.first; cy = best.second
        }
        return cx to cy
    }
}
