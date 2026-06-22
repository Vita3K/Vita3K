package org.vita3k.emulator.data

import androidx.annotation.StringRes
import org.vita3k.emulator.R

enum class RestartRequiredSetting(val nativeId: Int, @StringRes val labelResId: Int) {
    CpuOpt(0, R.string.settings_cpu_opt),
    BackendRenderer(1, R.string.settings_gpu_backend),
    GraphicsDevice(2, R.string.settings_gpu_graphics_device),
    CustomDriver(3, R.string.settings_gpu_custom_driver),
    HighAccuracy(4, R.string.settings_gpu_accuracy),
    ResolutionMultiplier(5, R.string.settings_gpu_resolution),
    MemoryMapping(6, R.string.settings_gpu_memory_mapping),
    AudioBackend(7, R.string.settings_audio_backend),
    ValidationLayer(8, R.string.settings_debug_validation_layer);

    companion object {
        fun fromNative(values: IntArray): List<RestartRequiredSetting> {
            if (values.isEmpty()) {
                return emptyList()
            }

            val byId = entries.associateBy(RestartRequiredSetting::nativeId)
            return buildList(values.size) {
                values.forEach { nativeId ->
                    byId[nativeId]?.let(::add)
                }
            }
        }
    }
}
