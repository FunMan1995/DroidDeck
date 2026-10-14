package com.droiddeck.launcher.session

import org.junit.Assert.*
import org.junit.Test

class RumbleTargetTest {
    @Test fun padRumblesItselfAndThePhoneOnlyForOnScreenControls() {
        assertEquals("pad", RumbleComponent.pick(true, "pad", "phone"))
        assertNull(RumbleComponent.pick(true, null, "phone"))
        assertEquals("phone", RumbleComponent.pick(false, null, "phone"))
    }
}
