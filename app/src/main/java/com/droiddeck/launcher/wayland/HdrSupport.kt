package com.droiddeck.launcher.wayland

import android.content.Context
import android.os.Build
import android.view.Display
import android.view.WindowManager
import com.droiddeck.launcher.R

class HdrProbe(
    val hdr10: Boolean,
    val formats: String,
    val maxLuminance: Float,
    val maxAverageLuminance: Float,
    val minLuminance: Float,
    val ratioAvailable: Boolean,
    val ratio: Float,
    val displayId: Int,
    val name: String,
    val reason: String?,
)

object HdrSupport {
    fun probe(context: Context): HdrProbe {
        val display: Display? = if (Build.VERSION.SDK_INT >= 30) {
            try { context.display } catch (e: Exception) { null }
        } else {
            @Suppress("DEPRECATION")
            (context.getSystemService(Context.WINDOW_SERVICE) as WindowManager).defaultDisplay
        }
        if (display == null) return HdrProbe(false, "", 0f, 0f, 0f, false, -1f, -1, "none", reason(context, false, ""))
        val caps = try { display.hdrCapabilities } catch (e: Exception) { null }
        val types = caps?.supportedHdrTypes ?: IntArray(0)
        val formats = types.joinToString(", ") { typeName(it) }
        val (ratioAvailable, ratio) = sdrRatio(Build.VERSION.SDK_INT,
            { Build.VERSION.SDK_INT >= 34 && display.isHdrSdrRatioAvailable },
            { if (Build.VERSION.SDK_INT >= 34) display.hdrSdrRatio else -1f })
        val hdr10 = types.contains(Display.HdrCapabilities.HDR_TYPE_HDR10)
        return HdrProbe(
            hdr10 = hdr10,
            formats = formats,
            maxLuminance = caps?.desiredMaxLuminance ?: 0f,
            maxAverageLuminance = caps?.desiredMaxAverageLuminance ?: 0f,
            minLuminance = caps?.desiredMinLuminance ?: 0f,
            ratioAvailable = ratioAvailable,
            ratio = ratio,
            displayId = display.displayId,
            name = display.name ?: "display",
            reason = reason(context, hdr10, formats),
        )
    }

    /** The API 34 HDR/SDR ratio path; any failure reads as unavailable (-1). */
    internal fun sdrRatio(sdk: Int, available: () -> Boolean, ratio: () -> Float): Pair<Boolean, Float> {
        val ok = sdk >= 34 && try { available() } catch (e: Exception) { false }
        val value = if (ok) try { ratio() } catch (e: Exception) { -1f } else -1f
        return ok to value
    }

    /** One-line probe summary for the session log (brief 005 §5: tags komodo, HDR stays opt-in). */
    fun logLine(probe: HdrProbe, device: String?, model: String?, wanted: Boolean): String {
        val tag = if (device == "komodo") "komodo (Pixel 9 Pro XL)" else "${device ?: "?"} (${model ?: "?"})"
        return String.format(java.util.Locale.US,
            "hdr probe on %s: display %d \"%s\" formats [%s] hdr10=%b lum max %.0f avg %.0f min %.3f sdrRatio %s%s; requested=%b%s",
            tag, probe.displayId, probe.name, probe.formats, probe.hdr10,
            probe.maxLuminance, probe.maxAverageLuminance, probe.minLuminance,
            if (probe.ratioAvailable) "available" else "unavailable",
            if (probe.ratioAvailable) String.format(java.util.Locale.US, " (%.2f)", probe.ratio) else "",
            wanted, probe.reason?.let { " (off: $it)" } ?: "")
    }

    private fun reason(context: Context, hdr10: Boolean, formats: String): String? = when {
        Build.VERSION.SDK_INT < 29 -> context.getString(R.string.hdr_needs_android_10)
        !hdr10 -> if (formats.isEmpty()) context.getString(R.string.hdr_none) else context.getString(R.string.hdr_not_hdr10, formats)
        else -> null
    }

    private fun typeName(t: Int): String = when (t) {
        Display.HdrCapabilities.HDR_TYPE_DOLBY_VISION -> "Dolby Vision"
        Display.HdrCapabilities.HDR_TYPE_HDR10 -> "HDR10"
        Display.HdrCapabilities.HDR_TYPE_HLG -> "HLG"
        Display.HdrCapabilities.HDR_TYPE_HDR10_PLUS -> "HDR10+"
        else -> "HDR"
    }
}
