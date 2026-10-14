package com.droiddeck.launcher.input

import android.hardware.input.InputManager
import android.view.InputDevice
import android.view.KeyCharacterMap
import android.view.KeyEvent
import android.view.MotionEvent
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import org.robolectric.shadows.InputDeviceBuilder

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [34], manifest = Config.NONE, shadows = [PadBridgeRoutingTest.RegisteredKeyEvent::class,
    PadBridgeRoutingTest.RegisteredMotionEvent::class])
class DualSenseInputTest {
    private val pad = InputDevice.SOURCE_GAMEPAD or InputDevice.SOURCE_JOYSTICK or InputDevice.SOURCE_KEYBOARD

    @Test fun volumeKeysPassThroughAsSystemVolumeFromAnyDevice() {
        val volumes = listOf(KeyEvent.KEYCODE_VOLUME_UP, KeyEvent.KEYCODE_VOLUME_DOWN, KeyEvent.KEYCODE_VOLUME_MUTE)
        val dualSense = device(1, pad, 0x054c, 0x0ce6, axes = listOf(MotionEvent.AXIS_X))
        val phoneKeys = device(2, InputDevice.SOURCE_KEYBOARD, 0, 0, keyboard = InputDevice.KEYBOARD_TYPE_NON_ALPHABETIC)
        val bridge = PadBridge(java.io.File(RuntimeEnvironment.getApplication().cacheDir, "fake-input"))
        for (code in volumes) {
            assertTrue(PadBridge.isSystemVolumeKey(code))
            for (device in listOf(dualSense, phoneKeys)) for (action in listOf(KeyEvent.ACTION_DOWN, KeyEvent.ACTION_UP)) {
                val event = KeyEvent(0L, 0L, action, code, 0, 0, device.id, 0, 0, InputDevice.SOURCE_GAMEPAD)
                assertFalse(PadBridge.isControllerKeyEvent(event))
                assertFalse(PadBridge.isControllerKey(event))
                assertFalse("volume $code must not reach the guest pad", bridge.onKeyEvent(event))
            }
            assertFalse(PadBridge.isPadKey(code, InputDevice.KEYBOARD_TYPE_NON_ALPHABETIC))
        }
        assertFalse(PadBridge.isSystemVolumeKey(KeyEvent.KEYCODE_BUTTON_A))
    }

    @Test fun dualSenseButtonsStillMapAsAPad() {
        val dualSense = device(3, pad, 0x054c, 0x0ce6, axes = listOf(MotionEvent.AXIS_X))
        assertTrue(PadBridge.isFromController(dualSense))
        for (code in listOf(KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_MODE, KeyEvent.KEYCODE_BUTTON_THUMBR)) {
            val event = KeyEvent(0L, 0L, KeyEvent.ACTION_DOWN, code, 0, 0, dualSense.id, 0, 0, InputDevice.SOURCE_GAMEPAD)
            assertSame(dualSense, event.device)
            assertTrue(PadBridge.isControllerKeyEvent(event))
        }
    }

    @Test fun dualSenseOnGenericLayoutUsesRxRyAndZRzTriggers() {
        val generic = device(4, pad, 0x054c, 0x0ce6,
            axes = listOf(MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ, MotionEvent.AXIS_RX, MotionEvent.AXIS_RY))
        assertArrayEquals(floatArrayOf(-1f, -1f), PadBridge.sonyLayoutOf(generic), 0f)
        val vendorKl = device(5, pad, 0x054c, 0x0ce6,
            axes = listOf(MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER))
        assertEquals(0, PadBridge.sonyLayoutOf(vendorKl).size)
        val xbox = device(6, pad, 0x045e, 0x0b13,
            axes = listOf(MotionEvent.AXIS_X, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ, MotionEvent.AXIS_RX, MotionEvent.AXIS_RY))
        assertEquals(0, PadBridge.sonyLayoutOf(xbox).size)
    }

    @Test fun theDualSenseTouchpadIsNotAPad() {
        val touchpad = device(7, InputDevice.SOURCE_TOUCHPAD or InputDevice.SOURCE_MOUSE, 0x054c, 0x0ce6,
            keyboard = InputDevice.KEYBOARD_TYPE_NONE)
        assertFalse(PadBridge.isFromController(touchpad))
    }

    private fun device(id: Int, sources: Int, vid: Int, pid: Int, axes: List<Int> = emptyList(),
        keyboard: Int = InputDevice.KEYBOARD_TYPE_NON_ALPHABETIC): InputDevice {
        val builder = InputDeviceBuilder.newBuilder().setId(id).setName("DualSense fixture $id")
            .setDescriptor("ds-$id").setExternal(true).setSources(sources).setKeyboardType(keyboard)
            .setVendorId(vid).setProductId(pid)
            .setKeyCharacterMap(KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD))
        for (axis in axes) builder.addMotionRange(axis, InputDevice.SOURCE_JOYSTICK, -1f, 1f, 0f, 0f, 0f)
        val device = builder.build()
        val manager = RuntimeEnvironment.getApplication().getSystemService(InputManager::class.java)
        shadowOf(manager).addInputDevice(device)
        shadowOf(manager).addInputDeviceKeys(id, intArrayOf(KeyEvent.KEYCODE_BUTTON_A))
        return device
    }
}
