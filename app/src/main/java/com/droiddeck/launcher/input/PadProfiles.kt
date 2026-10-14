package com.droiddeck.launcher.input

/**
 * Brief 007 §1: known controllers by USB/BT vendor:product, for the guest pad identity
 * (FAKE_EVDEV_IDENTITY per slot) and for the Android-side axis layout.
 * Unknown pads stay "xbox360"; nothing here changes how an unlisted pad behaves.
 */
object PadProfiles {
    const val XBOX360 = "xbox360"
    const val XBOXONE = "xboxone"
    const val PS5 = "ps5"
    const val PS4 = "ps4"

    const val SONY = 0x054c
    const val MICROSOFT = 0x045e
    const val EIGHTBITDO = 0x2dc8
    const val BACKBONE = 0x358a

    private val table: Map<Int, String> = mapOf(
        key(SONY, 0x0ce6) to PS5,       // DualSense
        key(SONY, 0x0df2) to PS5,       // DualSense Edge
        key(SONY, 0x05c4) to PS4,       // DualShock 4 (v1)
        key(SONY, 0x09cc) to PS4,       // DualShock 4 (v2)
        key(MICROSOFT, 0x0b13) to XBOXONE, // Xbox Series X|S (Bluetooth)
        key(MICROSOFT, 0x0b12) to XBOXONE, // Xbox Series X|S (USB)
        key(MICROSOFT, 0x0b20) to XBOXONE, // Xbox Series X|S (BLE, newer firmware)
        key(MICROSOFT, 0x02ea) to XBOXONE, // Xbox One S (USB)
        key(MICROSOFT, 0x0b00) to XBOXONE, // Xbox Elite 2 (USB)
    )

    private fun key(vid: Int, pid: Int) = (vid and 0xffff shl 16) or (pid and 0xffff)

    fun identityFor(vid: Int, pid: Int): String =
        table[key(vid, pid)] ?: XBOX360 // 8BitDo, Backbone and unknown pads: xbox360 (xpad layout)

    fun isDualSense(vid: Int, pid: Int): Boolean = identityFor(vid, pid) == PS5

    /** Guest-side USB ids and evdev name the identity presents (hid-playstation / xpad names). */
    data class Presented(val vendor: Int, val product: Int, val name: String)

    fun presented(identity: String): Presented = when (identity) {
        PS5 -> Presented(SONY, 0x0ce6, "Sony Interactive Entertainment DualSense Wireless Controller")
        PS4 -> Presented(SONY, 0x09cc, "Sony Interactive Entertainment Wireless Controller")
        XBOXONE -> Presented(MICROSOFT, 0x0b12, "Microsoft Xbox Series S|X Controller")
        else -> Presented(MICROSOFT, 0x028e, "Xbox 360 Controller")
    }

    /**
     * Sony pads on kernels/key layouts without the vendor .kl show up through Generic.kl:
     * right stick on AXIS_RX/AXIS_RY and the analog triggers on AXIS_Z/AXIS_RZ (hid-playstation's
     * ABS_Z / ABS_RZ). With the AOSP vendor layout they arrive as Z/RZ + LTRIGGER/RTRIGGER like
     * every other pad. Decide from the device's actual motion ranges.
     */
    fun sonyGenericLayout(vid: Int, hasRx: Boolean, hasRy: Boolean, hasLTrigger: Boolean, hasBrake: Boolean): Boolean =
        vid == SONY && hasRx && hasRy && !hasLTrigger && !hasBrake

    /** Maps a trigger axis to 0..1: -1..1 ranges (some Generic.kl Z/RZ) are rescaled. */
    fun trigger(value: Float, min: Float): Float =
        (if (min < -0.5f) (value + 1f) / 2f else value).coerceIn(0f, 1f)

    /** Writes $FAKE_EVDEV_DIR/identityN for the guest preload, replacing it atomically. */
    fun writeIdentityFile(target: java.io.File, identity: String): Boolean {
        val tmp = java.io.File(target.path + ".tmp")
        return try {
            tmp.writeText(identity + "\n", Charsets.US_ASCII)
            tmp.renameTo(target)
        } catch (e: java.io.IOException) {
            android.util.Log.w("PadProfiles", "identity ${target.name}: $e")
            false
        }
    }
}
