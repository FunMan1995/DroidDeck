package com.droiddeck.launcher.input

import org.junit.Assert.assertEquals
import org.junit.Test

class CutoutAvoidTest {
    // komodo-like landscape: centred punch hole on the left edge (camera at the top in portrait).
    private val hole = listOf(intArrayOf(0, 620, 100, 724))

    @Test fun controlClearOfTheHoleStays() {
        assertEquals(400f to 300f, CutoutAvoid.nudge(400f, 300f, 60f, 60f, hole, 2992, 1344))
        assertEquals(400f to 300f, CutoutAvoid.nudge(400f, 300f, 60f, 60f, emptyList(), 2992, 1344))
    }

    @Test fun controlOverTheHoleMovesTheShortestWayOff() {
        assertEquals(150f to 672f, CutoutAvoid.nudge(80f, 672f, 50f, 50f, hole, 2992, 1344))
        assertEquals(60f to 774f, CutoutAvoid.nudge(60f, 700f, 50f, 50f, hole, 2992, 1344))
    }

    @Test fun marginIsKeptAndScreenEdgesRespected() {
        assertEquals(154f to 672f, CutoutAvoid.nudge(80f, 672f, 50f, 50f, hole, 2992, 1344, margin = 4f))
        val corner = listOf(intArrayOf(0, 0, 100, 100))
        assertEquals(50f to 150f, CutoutAvoid.nudge(50f, 50f, 50f, 50f, corner, 2992, 1344))
    }
}
