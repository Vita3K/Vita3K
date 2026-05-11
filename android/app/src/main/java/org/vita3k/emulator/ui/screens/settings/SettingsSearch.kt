package org.vita3k.emulator.ui.screens.settings

import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import org.vita3k.emulator.R

@Composable
internal fun rememberSettingsSearchEntries(
    isPerApp: Boolean,
    backendRenderer: String,
    showCustomDriverOptions: Boolean,
    showTurboModeOption: Boolean = showCustomDriverOptions
): List<SettingsSearchEntry> {
    val context = LocalContext.current
    val configuration = LocalConfiguration.current

    fun help(
        @StringRes titleRes: Int,
        @StringRes bodyRes: Int,
        scope: SettingsScope = SettingsScope.Both
    ): SettingsHelpEntry {
        return SettingsHelpEntry(
            title = context.getString(titleRes),
            body = context.getString(bodyRes),
            scope = scope
        )
    }

    fun entry(
        category: SettingsCategory,
        @StringRes titleRes: Int,
        @StringRes bodyRes: Int? = null,
        scope: SettingsScope = SettingsScope.Both,
        keywords: String = ""
    ): SettingsSearchEntry {
        val title = context.getString(titleRes)
        val categoryLabel = context.getString(category.labelRes)
        val helpEntry = bodyRes?.let { help(titleRes, it, scope) }
        val searchableBody = bodyRes?.let { context.getString(it) }.orEmpty()
        return SettingsSearchEntry(
            category = category,
            title = title,
            summary = categoryLabel,
            keywords = listOf(title, categoryLabel, searchableBody, keywords).joinToString(" "),
            help = helpEntry
        )
    }

    val isVulkan = remember(backendRenderer) { isVulkanBackend(backendRenderer) }

    return remember(context, configuration, isPerApp, isVulkan, showCustomDriverOptions, showTurboModeOption) { buildList {
        add(entry(SettingsCategory.Core, R.string.settings_modules_mode, R.string.settings_modules_automatic_desc))
        add(entry(SettingsCategory.Core, R.string.settings_modules_list_title, keywords = "lle modules firmware"))
        add(entry(SettingsCategory.Cpu, R.string.settings_cpu_opt, R.string.settings_cpu_opt_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_backend, R.string.settings_gpu_backend_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_accuracy, R.string.settings_gpu_accuracy_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_vsync, R.string.settings_gpu_vsync_desc))
        if (isVulkan) {
            add(entry(SettingsCategory.Gpu, R.string.settings_gpu_disable_surface_sync, R.string.settings_gpu_disable_surface_sync_desc))
        }
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_async_pipeline, R.string.settings_gpu_async_pipeline_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_screen_filter, R.string.settings_gpu_screen_filter_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_resolution, R.string.settings_gpu_resolution_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_anisotropic, R.string.settings_gpu_anisotropic_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_export_textures, R.string.settings_gpu_export_textures_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_texture_format, R.string.settings_gpu_texture_format_desc, keywords = "png dds export textures"))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_import_textures, R.string.settings_gpu_import_textures_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_shader_cache, R.string.settings_gpu_shader_cache_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_spirv_shader, R.string.settings_gpu_spirv_shader_desc))
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_fps_hack, R.string.settings_gpu_fps_hack_desc, keywords = "fps framerate 30 60 game hack"))
        if (!isPerApp && showTurboModeOption) {
            add(entry(SettingsCategory.Gpu, R.string.settings_gpu_turbo_mode, R.string.settings_gpu_turbo_mode_desc, scope = SettingsScope.Global, keywords = "gpu clocks adreno thermal"))
        }
        add(entry(SettingsCategory.Gpu, R.string.settings_gpu_memory_mapping, R.string.settings_gpu_memory_mapping_desc))
        if (showCustomDriverOptions) {
            add(entry(SettingsCategory.Gpu, R.string.settings_gpu_custom_driver, R.string.settings_gpu_custom_driver_desc, keywords = "adreno driver zip"))
        }
        add(entry(SettingsCategory.Audio, R.string.settings_audio_backend, R.string.settings_audio_backend_desc))
        add(entry(SettingsCategory.Audio, R.string.settings_audio_volume, R.string.settings_audio_volume_desc))
        add(entry(SettingsCategory.Audio, R.string.settings_audio_ngs, R.string.settings_audio_ngs_desc))
        if (!isPerApp) {
            add(entry(SettingsCategory.Camera, R.string.settings_camera_front, R.string.settings_camera_front_desc, scope = SettingsScope.Global, keywords = "camera source solid color static image"))
            add(entry(SettingsCategory.Camera, R.string.settings_camera_back, R.string.settings_camera_back_desc, scope = SettingsScope.Global, keywords = "camera source solid color static image"))
            add(entry(SettingsCategory.Camera, R.string.settings_camera_image, R.string.settings_camera_image_desc, scope = SettingsScope.Global, keywords = "front back camera picture image"))
            add(entry(SettingsCategory.Camera, R.string.settings_camera_color, R.string.settings_camera_color_desc, scope = SettingsScope.Global, keywords = "front back camera color red green blue alpha"))
        }
        add(entry(SettingsCategory.System, R.string.settings_system_pstv_mode, R.string.settings_system_pstv_mode_desc))
        if (!isPerApp) {
            add(entry(SettingsCategory.System, R.string.settings_system_show_mode, R.string.settings_system_show_mode_desc, scope = SettingsScope.Global))
            add(entry(SettingsCategory.System, R.string.settings_system_demo_mode, R.string.settings_system_demo_mode_desc, scope = SettingsScope.Global))
        }
        add(entry(SettingsCategory.System, R.string.settings_system_enter_button, R.string.settings_system_enter_button_desc))
        add(entry(SettingsCategory.System, R.string.settings_system_language, R.string.settings_system_language_desc))
        add(entry(SettingsCategory.System, R.string.settings_system_date_format, R.string.settings_system_date_format_desc))
        add(entry(SettingsCategory.System, R.string.settings_system_time_format, R.string.settings_system_time_format_desc))
        add(entry(SettingsCategory.System, R.string.settings_system_ime_langs, R.string.settings_system_ime_langs_desc))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_show, R.string.settings_controls_show_desc, scope = SettingsScope.Global, keywords = "overlay touch buttons"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_hide_on_controller, R.string.settings_controls_hide_on_controller_desc, scope = SettingsScope.Global, keywords = "gamepad controller overlay hide"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_l2r2, R.string.settings_controls_l2r2_desc, scope = SettingsScope.Global, keywords = "touch triggers shoulder buttons"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_l3r3, R.string.settings_controls_l3r3_desc, scope = SettingsScope.Global, keywords = "touch stick clicks l3 r3"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_touch_switch, R.string.settings_controls_touch_switch_desc, scope = SettingsScope.Global, keywords = "front rear touchscreen switch"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_hide_toggle, R.string.settings_controls_hide_toggle_desc, scope = SettingsScope.Global, keywords = "hide overlay button toggle"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_scale, R.string.settings_controls_scale_desc, scope = SettingsScope.Global, keywords = "overlay size"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_opacity, R.string.settings_controls_opacity_desc, scope = SettingsScope.Global, keywords = "overlay transparency"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_mapping_buttons_title, R.string.settings_controls_mapping_buttons_desc, scope = SettingsScope.Global, keywords = "controller remap mapping buttons gamepad"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_mapping_axes_title, R.string.settings_controls_mapping_axes_desc, scope = SettingsScope.Global, keywords = "controller remap sticks triggers axes gamepad"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_analog_multiplier, R.string.settings_controls_analog_multiplier_desc, scope = SettingsScope.Global, keywords = "analog stick sensitivity multiplier"))
        add(entry(SettingsCategory.Controls, R.string.settings_controls_disable_motion, R.string.settings_controls_disable_motion_desc, scope = SettingsScope.Global, keywords = "gyro accelerometer motion controls"))
        add(entry(SettingsCategory.Network, R.string.settings_network_psn_signed_in, R.string.settings_network_psn_signed_in_desc, scope = SettingsScope.PerApp))
        if (!isPerApp) {
            add(entry(SettingsCategory.Network, R.string.settings_network_http_enable, R.string.settings_network_http_enable_desc, scope = SettingsScope.Global))
            add(entry(SettingsCategory.Network, R.string.settings_network_http_timeout_attempts, R.string.settings_network_http_timeout_attempts_desc, scope = SettingsScope.Global, keywords = "retry timeout http"))
            add(entry(SettingsCategory.Network, R.string.settings_network_http_timeout_sleep, R.string.settings_network_http_timeout_sleep_desc, scope = SettingsScope.Global, keywords = "retry delay timeout http ms"))
            add(entry(SettingsCategory.Network, R.string.settings_network_http_read_end_attempts, R.string.settings_network_http_read_end_attempts_desc, scope = SettingsScope.Global, keywords = "read end retry http"))
            add(entry(SettingsCategory.Network, R.string.settings_network_http_read_end_sleep, R.string.settings_network_http_read_end_sleep_desc, scope = SettingsScope.Global, keywords = "read end delay http ms"))
            add(entry(SettingsCategory.Network, R.string.settings_network_adhoc_address, R.string.settings_network_adhoc_address_desc, scope = SettingsScope.Global, keywords = "adhoc subnet local address"))
        }
        if (!isPerApp) {
            add(entry(SettingsCategory.Debug, R.string.settings_debug_log_imports, R.string.settings_debug_log_imports_desc, scope = SettingsScope.Global, keywords = "import symbols hle"))
            add(entry(SettingsCategory.Debug, R.string.settings_debug_log_exports, R.string.settings_debug_log_exports_desc, scope = SettingsScope.Global, keywords = "export symbols hle"))
        }
        add(entry(SettingsCategory.Debug, R.string.settings_debug_log_active_shaders, R.string.settings_debug_log_active_shaders_desc))
        add(entry(SettingsCategory.Debug, R.string.settings_debug_log_uniforms, R.string.settings_debug_log_uniforms_desc))
        add(entry(SettingsCategory.Debug, R.string.settings_debug_color_surface, R.string.settings_debug_color_surface_desc))
        add(entry(SettingsCategory.Debug, R.string.settings_debug_validation_layer, R.string.settings_debug_validation_layer_desc))
        if (!isPerApp) {
            add(entry(SettingsCategory.Debug, R.string.settings_debug_dump_elfs, R.string.settings_debug_dump_elfs_desc, scope = SettingsScope.Global, keywords = "elf dump loaded code"))
        }
        add(entry(SettingsCategory.Emulator, R.string.settings_emulator_texture_cache, R.string.settings_emulator_texture_cache_desc))
        add(entry(SettingsCategory.Emulator, R.string.settings_emulator_stretch_display, R.string.settings_emulator_stretch_display_desc))
        add(entry(SettingsCategory.Emulator, R.string.settings_emulator_pixel_perfect, R.string.settings_emulator_pixel_perfect_desc))
        add(entry(SettingsCategory.Emulator, R.string.settings_emulator_file_loading_delay, R.string.settings_emulator_file_loading_delay_desc))
        if (!isPerApp) {
            add(entry(SettingsCategory.Interface, R.string.settings_emulator_ui_language, R.string.settings_emulator_ui_language_desc, scope = SettingsScope.Global, keywords = "interface ui app language locale"))
            add(entry(SettingsCategory.Emulator, R.string.settings_emulator_storage_folder, R.string.settings_emulator_storage_folder_desc, scope = SettingsScope.Global, keywords = "storage folder path data directory"))
            add(entry(SettingsCategory.Emulator, R.string.settings_emulator_show_compile_shaders, R.string.settings_emulator_show_compile_shaders_desc, scope = SettingsScope.Global))
            add(entry(SettingsCategory.Emulator, R.string.settings_emulator_check_updates, R.string.settings_emulator_check_updates_desc, scope = SettingsScope.Global))
            add(entry(SettingsCategory.Emulator, R.string.settings_emulator_archive_log, R.string.settings_emulator_archive_log_desc, scope = SettingsScope.Global))
            add(entry(SettingsCategory.Emulator, R.string.settings_emulator_log_compat_warn, R.string.settings_emulator_log_compat_warn_desc, scope = SettingsScope.Global))
            add(entry(SettingsCategory.Emulator, R.string.settings_emulator_log_level, R.string.settings_emulator_log_level_desc, scope = SettingsScope.Global, keywords = "logging trace debug info warning error critical off"))
            add(entry(SettingsCategory.Emulator, R.string.settings_emulator_perf_overlay, R.string.settings_emulator_perf_overlay_desc, scope = SettingsScope.Global))
            add(entry(SettingsCategory.Emulator, R.string.settings_emulator_perf_overlay_detail, R.string.settings_emulator_perf_overlay_detail_desc, scope = SettingsScope.Global, keywords = "minimum low medium maximum"))
            add(entry(SettingsCategory.Emulator, R.string.settings_emulator_perf_overlay_position, R.string.settings_emulator_perf_overlay_position_desc, scope = SettingsScope.Global, keywords = "top bottom left center right"))
            add(entry(SettingsCategory.Emulator, R.string.settings_emulator_screenshot_format, R.string.settings_emulator_screenshot_format_desc, scope = SettingsScope.Global, keywords = "jpeg png none"))
        }
    } }
}

@Composable
internal fun SettingsSearchResults(
    query: String,
    entries: List<SettingsSearchEntry>,
    onOpen: (SettingsCategory) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    val normalizedQuery = remember(query) { normalizeSettingsSearchToken(query) }
    val filtered = remember(entries, normalizedQuery) {
        entries.filter { entry ->
            normalizeSettingsSearchToken(entry.keywords).contains(normalizedQuery)
        }
    }

    SettingsSectionCard(
        title = stringResource(R.string.settings_search_results),
        summary = if (filtered.isEmpty()) stringResource(R.string.settings_search_no_results) else null,
        help = null,
        onShowHelp = onShowHelp
    ) {
        if (filtered.isEmpty()) {
            SettingsNote(text = stringResource(R.string.settings_search_no_results))
        } else {
            filtered.forEach { entry ->
                SettingsSearchResultRow(entry = entry, onOpen = onOpen, onShowHelp = onShowHelp)
            }
        }
    }
}

private fun normalizeSettingsSearchToken(value: String): String {
    return value
        .lowercase()
        .replace(Regex("[^a-z0-9]+"), " ")
        .trim()
}
