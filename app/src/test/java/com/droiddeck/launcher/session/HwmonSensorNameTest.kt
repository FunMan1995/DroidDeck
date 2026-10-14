package com.droiddeck.launcher.session

import org.junit.Assert.assertEquals
import org.junit.Test

class HwmonSensorNameTest {
    @Test fun adrenoKeepsTheQualcommNameSteamsHudExpects() {
        assertEquals("gpuss0_thermal", HwmonComponent.gpuSensorName(adreno = true))
    }

    @Test fun otherGpusGetANeutralName() {
        assertEquals("gpu_thermal", HwmonComponent.gpuSensorName(adreno = false))
    }
}
