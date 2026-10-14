package com.droiddeck.launcher.core

import android.content.Context
import android.os.Build
import com.droiddeck.launcher.R
import com.droiddeck.launcher.gpu.GpuInfo
import java.io.File

object DeviceSupport {
    fun adreno(): Boolean =
        File("/sys/class/kgsl/kgsl-3d0").exists() || File("/vendor/lib64/hw/vulkan.adreno.so").exists()

    /** GPU vendor from public signals; an Adreno kernel node always wins. */
    fun gpuVendor(): GpuInfo.Vendor = vendorFor(adreno(), GpuInfo.vendorOf(signals()))

    fun mali(): Boolean = gpuVendor() == GpuInfo.Vendor.MALI

    /** Turnip and the Adreno driver pairs are only offered on Adreno. */
    fun turnipAllowed(): Boolean = gpuVendor() == GpuInfo.Vendor.ADRENO

    internal fun vendorFor(adrenoNode: Boolean, fromSignals: GpuInfo.Vendor): GpuInfo.Vendor =
        if (adrenoNode) GpuInfo.Vendor.ADRENO else fromSignals

    /** Recognized GPUs are Adreno and Mali; anything else is unsupported. */
    fun recognized(vendor: GpuInfo.Vendor): Boolean = vendor != GpuInfo.Vendor.UNKNOWN

    private fun signals(): GpuInfo.Signals {
        val soc = Build.VERSION.SDK_INT >= 31
        return GpuInfo.Signals(
            socManufacturer = if (soc) Build.SOC_MANUFACTURER.takeIf { it != Build.UNKNOWN }.orEmpty() else "",
            socModel = if (soc) Build.SOC_MODEL.takeIf { it != Build.UNKNOWN }.orEmpty() else "",
            boardPlatform = GpuInfo.systemProperty("ro.board.platform"),
        )
    }

    fun gpuName(context: Context): String {
        val soc = if (Build.VERSION.SDK_INT >= 31) Build.SOC_MODEL.takeIf { it.isNotBlank() && it != Build.UNKNOWN } else null
        return soc?.let { "$it (${Build.HARDWARE})" } ?: Build.HARDWARE.ifBlank { context.getString(R.string.gpu_this_gpu) }
    }

    fun lowLatencyWifiSafe(): Boolean = Build.VERSION.SDK_INT < 31 || Build.SOC_MODEL != "CQ8725S"
}
