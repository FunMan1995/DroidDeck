package com.droiddeck.launcher.ui

import com.droiddeck.launcher.session.SessionDisplay
import com.droiddeck.launcher.ui.GamepadGeometry.Slot
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GamepadGeometryTest {
    private data class Panel(val name: String, val width: Int, val height: Int, val density: Float) {
        val controlsHeight get() = height - SessionDisplay.unfoldedGameHeight(width, height)
    }

    private val panels = listOf(
        Panel("Fold 8 upright", 1848, 2448, 2.625f),
        Panel("Fold 8 as a tablet", 2448, 1848, 2.625f),
        Panel("Pixel Fold upright", 1840, 2208, 2.625f),
        Panel("Pixel Fold as a tablet", 2208, 1840, 2.625f),
        Panel("OnePlus Open upright", 2268, 2440, 2.75f),
        Panel("OnePlus Open as a tablet", 2440, 2268, 2.75f),
        Panel("Fold 8 upright, large display size", 1848, 2448, 3.5f),
        Panel("Fold 8 as a tablet, small display size", 2448, 1848, 2.0f),
    )

    private fun boxes(panel: Panel) =
        GamepadGeometry.layout(panel.width.toFloat(), panel.controlsHeight.toFloat(), panel.density)

    @Test fun everyControlIsPlacedAndPanelsOnlyWhenUpright() {
        for (panel in panels) {
            val upright = panel.height > panel.width
            val expected = Slot.values().toSet() - if (upright) emptySet() else setOf(Slot.PANELS)
            assertEquals(panel.name, expected, boxes(panel).keys)
        }
    }

    @Test fun everyControlStaysOnThePanel() {
        for (panel in panels) for ((slot, box) in boxes(panel)) {
            val where = "${panel.name}: $slot $box"
            assertTrue(where, box.left >= 0f && box.top >= 0f)
            assertTrue(where, box.right <= panel.width && box.bottom <= panel.controlsHeight)
        }
    }

    @Test fun noControlsOverlap() {
        for (panel in panels) {
            val placed = boxes(panel).entries.toList()
            for (i in placed.indices) for (j in i + 1 until placed.size) {
                assertFalse("${panel.name}: ${placed[i].key} overlaps ${placed[j].key}",
                    placed[i].value.overlaps(placed[j].value))
            }
        }
    }

    @Test fun everyControlIsBigEnoughForAThumb() {
        for (panel in panels) for ((slot, box) in boxes(panel)) {
            val minPx = 40 * panel.density
            assertTrue("${panel.name}: $slot is ${box.width}x${box.height}, under 40dp",
                box.width >= minPx - 1 && box.height >= minPx - 1)
        }
    }

    @Test fun mainControlsSitUnderTheThumbsAndMirror() {
        for (panel in panels) {
            val b = boxes(panel)
            val left = b.getValue(Slot.LEFT_MAIN)
            val right = b.getValue(Slot.RIGHT_MAIN)
            assertEquals(panel.name, left.cy, right.cy, 0.5f)
            assertEquals(panel.name, left.cx, panel.width - right.cx, 0.5f)
            assertTrue(panel.name, left.cx < b.getValue(Slot.LEFT_SECOND).cx)
            assertTrue(panel.name, left.cy > panel.controlsHeight / 2f)
        }
    }

    @Test fun middleButtonsKeepTheDeckOrder() {
        for (panel in panels) {
            val b = boxes(panel)
            fun before(a: Slot, c: Slot) = b.getValue(a).let { x -> b.getValue(c).let { y ->
                x.cx < y.cx - 0.5f || (kotlin.math.abs(x.cx - y.cx) <= 0.5f && x.cy < y.cy) } }
            assertTrue(panel.name, before(Slot.VIEW, Slot.MENU))
            assertTrue(panel.name, before(Slot.STEAM, Slot.QAM))
        }
    }

    @Test fun komodoDefaultLayoutAvoidsTheCentredCameraHole() {
        // 2992x1344 at density ~3.0; the centred punch hole sits mid-edge in landscape, top-centre in portrait.
        val cases = listOf(
            Triple(2992f, 1344f, intArrayOf(20, 620, 120, 724)),
            Triple(2992f, 1344f, intArrayOf(2872, 620, 2972, 724)),
            Triple(1344f, 2992f, intArrayOf(620, 20, 724, 120)),
        )
        for ((w, h, hole) in cases) {
            val boxes = GamepadGeometry.layout(w, h, 3f, listOf(hole))
            assertEquals(GamepadGeometry.layout(w, h, 3f).keys, boxes.keys)
            val rect = GamepadGeometry.Box((hole[0] + hole[2]) / 2f, (hole[1] + hole[3]) / 2f,
                (hole[2] - hole[0]).toFloat(), (hole[3] - hole[1]).toFloat())
            for ((slot, box) in boxes) {
                assertFalse("$slot overlaps the hole at ${w}x$h", box.overlaps(rect))
                assertTrue("$slot off screen", box.left >= -0.5f && box.top >= -0.5f && box.right <= w + 0.5f && box.bottom <= h + 0.5f)
            }
        }
        assertEquals(GamepadGeometry.layout(2992f, 1344f, 3f), GamepadGeometry.layout(2992f, 1344f, 3f, emptyList()))
    }
}
