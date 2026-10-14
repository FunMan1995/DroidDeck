package com.droiddeck.launcher.gpu

import android.content.Context
import android.os.Build
import com.droiddeck.launcher.R
import com.droiddeck.launcher.core.FileUtils
import java.io.File

data class GpuInfo(
    val name: String,
    val model: Int,
    val family: Family,
    val soc: String,
    val oneUi8Gen2: Boolean,
    val kgslName: String = "",
    val modelSource: String = "",
) {
    enum class Family(val label: String) {
        A8XX("Adreno 8xx"),
        A7XX_LOW("Adreno 710/720/722"),
        A7XX("Adreno 7xx"),
        A6XX("Adreno 6xx"),
        ADRENO_UNKNOWN("Adreno"),
        MALI("Arm Mali"),
        NOT_ADRENO("Not an Adreno GPU");

        fun label(context: Context): String = if (this == NOT_ADRENO) context.getString(R.string.gpuinfo_not_adreno) else label
    }

    /** GPU vendor, from public signals only (renderer/vendor strings, Vulkan vendorID, SoC properties). */
    enum class Vendor { ADRENO, MALI, UNKNOWN }

    val vendor: Vendor
        get() = when (family) {
            Family.MALI -> Vendor.MALI
            Family.NOT_ADRENO -> Vendor.UNKNOWN
            else -> Vendor.ADRENO
        }

    /** Turnip (and every Adreno driver pair) only applies to Adreno GPUs. */
    val turnipCapable: Boolean get() = vendor == Vendor.ADRENO

    /** Public SoC signals (Build.SOC_MANUFACTURER, Build.SOC_MODEL, ro.board.platform) plus optional GL/Vulkan strings. */
    data class Signals(
        val socManufacturer: String = "",
        val socModel: String = "",
        val boardPlatform: String = "",
        val renderer: String = "",
        val glVendor: String = "",
        val vulkanVendorId: Int? = null,
    )

    enum class Support { TESTED, UNTESTED, UNSUPPORTED }

    val support: Support
        get() = when {
            family == Family.NOT_ADRENO -> Support.UNSUPPORTED
            family == Family.MALI -> Support.UNTESTED
            family == Family.A6XX && model == 650 -> Support.TESTED
            family == Family.A8XX -> Support.TESTED
            family == Family.A7XX && model >= 725 -> Support.TESTED
            else -> Support.UNTESTED
        }

    val supportText: String
        get() = when (support) {
            Support.TESTED -> "Supported"
            Support.UNTESTED -> if (family == Family.MALI) "Experimental: Arm Mali is recognized, but Turnip drivers do not apply"
                else if (family == Family.A7XX_LOW) "Experimental: its drivers are test builds"
                else "Outside tested hardware (Adreno 650, 725 and newer): it may not run"
            Support.UNSUPPORTED -> "Not supported: DroidDeck needs an Adreno (Snapdragon) GPU"
        }

    fun supportText(context: Context): String = if (family == Family.MALI) supportText else context.getString(when (support) {
        Support.TESTED -> R.string.gpuinfo_supported
        Support.UNTESTED -> if (family == Family.A7XX_LOW) R.string.gpuinfo_experimental else R.string.gpuinfo_below_tested
        Support.UNSUPPORTED -> R.string.gpuinfo_unsupported
    })

    fun displayName(context: Context): String = if (name == UNNAMED) context.getString(R.string.gpu_this_gpu) else name

    companion object {
        private const val UNNAMED = "this GPU"

        fun detect(): GpuInfo {
            val adreno = File("/sys/class/kgsl/kgsl-3d0").exists() || File("/vendor/lib64/hw/vulkan.adreno.so").exists()
            if (!adreno) {
                val signals = currentSignals()
                if (vendorOf(signals) == Vendor.MALI) return maliInfo(signals)
            }
            val raw = listOf("/sys/class/kgsl/kgsl-3d0/gpu_model", "/sys/class/kgsl/kgsl-3d0/gpu_chipid")
                .firstNotNullOfOrNull { FileUtils.readString(File(it))?.trim()?.takeIf(String::isNotEmpty) }
            val soc = if (adreno) SocNames.label() else ""
            val fromKernel = raw?.let { threeDigits(it) } ?: 0
            val fromVulkan = if (fromKernel > 0 || !adreno) 0 else VulkanInfo.cachedOrNull()?.get("device")?.let { threeDigits(it) } ?: 0
            val fromPlatform = if (fromKernel > 0 || fromVulkan > 0 || !adreno) 0 else platformModel()
            val model = maxOf(fromKernel, fromVulkan, fromPlatform)
            val source = when {
                fromKernel > 0 -> "kernel"
                fromVulkan > 0 -> "vulkan"
                fromPlatform > 0 -> "platform"
                else -> ""
            }
            val family = familyOf(adreno, model)
            val samsung = Build.MANUFACTURER.equals("samsung", ignoreCase = true)
            return GpuInfo(
                name = if (!adreno) Build.HARDWARE.ifBlank { UNNAMED } else if (model > 0) "Adreno $model" else "Adreno",
                model = model, family = family, soc = soc,
                oneUi8Gen2 = samsung && model == 740,
                kgslName = raw.orEmpty(), modelSource = source,
            )
        }

        const val ARM_VULKAN_VENDOR_ID = 0x13B5
        const val QUALCOMM_VULKAN_VENDOR_ID = 0x5143

        /** Google Tensor board platforms with an Arm Mali GPU. Tensor G5 (laguna) is not Mali and is left out. */
        private val TENSOR_MALI = mapOf(
            "gs101" to TensorSoc("Tensor", "Mali-G78"),
            "gs201" to TensorSoc("Tensor G2", "Mali-G710"),
            "zuma" to TensorSoc("Tensor G3", "Mali-G715"),
            "zumapro" to TensorSoc("Tensor G4", "Mali-G715"),
        )
        private val TENSOR_MODELS = mapOf(
            "gs101" to "gs101", "tensor" to "gs101",
            "gs201" to "gs201", "tensor g2" to "gs201",
            "zuma" to "zuma", "gs301" to "zuma", "tensor g3" to "zuma",
            "zuma pro" to "zumapro", "zumapro" to "zumapro", "gs401" to "zumapro", "tensor g4" to "zumapro",
        )

        data class TensorSoc(val name: String, val gpu: String)

        /** The Tensor SoC behind these signals, or null when it isn't a known Mali Tensor. */
        fun tensorOf(signals: Signals): TensorSoc? {
            val platform = signals.boardPlatform.trim().lowercase()
            TENSOR_MALI[platform]?.let { return it }
            val google = signals.socManufacturer.trim().equals("google", ignoreCase = true)
            val key = TENSOR_MODELS[signals.socModel.trim().lowercase()] ?: return null
            return if (google || signals.socModel.trim().lowercase().startsWith("tensor")) TENSOR_MALI[key] else null
        }

        fun vendorOf(signals: Signals): Vendor {
            val strings = "${signals.renderer} ${signals.glVendor}".lowercase()
            return when {
                "adreno" in strings || "qualcomm" in strings -> Vendor.ADRENO
                signals.vulkanVendorId == QUALCOMM_VULKAN_VENDOR_ID -> Vendor.ADRENO
                "mali" in strings || Regex("""\barm\b""").containsMatchIn(strings) -> Vendor.MALI
                signals.vulkanVendorId == ARM_VULKAN_VENDOR_ID -> Vendor.MALI
                signals.vulkanVendorId != null -> Vendor.UNKNOWN
                tensorOf(signals) != null -> Vendor.MALI
                platformModel(signals.boardPlatform) > 0 -> Vendor.ADRENO
                else -> Vendor.UNKNOWN
            }
        }

        /** Mali GPU name, e.g. "Mali-G715", from the renderer string or the Tensor table. */
        fun maliName(signals: Signals): String =
            Regex("""(?i)(mali-?[a-z]?\d+)""").find(signals.renderer)?.groupValues?.get(1)
                ?.let { "Mali-" + it.substring(4).trimStart('-').uppercase() }
                ?: tensorOf(signals)?.gpu ?: "Mali"

        fun maliInfo(signals: Signals): GpuInfo {
            val tensor = tensorOf(signals)
            return GpuInfo(
                name = maliName(signals), model = 0, family = Family.MALI,
                soc = tensor?.name ?: signals.socModel, oneUi8Gen2 = false,
                modelSource = if (signals.renderer.isNotBlank() || signals.vulkanVendorId != null) "vulkan" else "platform",
            )
        }

        private fun currentSignals(): Signals {
            val vk = VulkanInfo.cachedOrNull()
            val soc = Build.VERSION.SDK_INT >= 31
            return Signals(
                socManufacturer = if (soc) Build.SOC_MANUFACTURER.takeIf { it != Build.UNKNOWN }.orEmpty() else "",
                socModel = if (soc) Build.SOC_MODEL.takeIf { it != Build.UNKNOWN }.orEmpty() else "",
                boardPlatform = systemProperty("ro.board.platform"),
                renderer = vk?.get("device").orEmpty(),
                vulkanVendorId = vk?.get("vendorID")?.let(::parseId),
            )
        }

        internal fun parseId(text: String): Int? = text.trim().let {
            if (it.startsWith("0x", ignoreCase = true)) it.substring(2).toIntOrNull(16) else it.toIntOrNull()
        }

        private fun threeDigits(text: String): Int = Regex("""(\d{3})""").find(text)?.groupValues?.get(1)?.toIntOrNull() ?: 0

        private val PLATFORMS = mapOf(
            "msmnile" to 640, "kona" to 650, "lahaina" to 660, "taro" to 730, "cape" to 730,
            "kalama" to 740, "pineapple" to 750, "sun" to 830,
        )

        internal fun platformModel(platform: String = systemProperty("ro.board.platform")): Int =
            PLATFORMS[platform.lowercase()] ?: PLATFORMS[systemProperty("ro.vendor.qti.soc_name").lowercase()] ?: 0

        fun systemProperty(name: String): String = try {
            Class.forName("android.os.SystemProperties").getMethod("get", String::class.java).invoke(null, name) as? String ?: ""
        } catch (_: Exception) {
            ""
        }

        internal fun familyOf(adreno: Boolean, model: Int): Family = when {
            !adreno -> Family.NOT_ADRENO
            model == 0 -> Family.ADRENO_UNKNOWN
            model >= 800 -> Family.A8XX
            model in listOf(710, 720, 722) -> Family.A7XX_LOW
            model >= 700 -> Family.A7XX
            model >= 600 -> Family.A6XX
            else -> Family.ADRENO_UNKNOWN
        }
    }
}
