// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <gui-qt/qt_utils.h>
#include <gui-qt/settings_dialog.h>
#include <gui-qt/settings_dialog_tooltips.h>

#include <QCoreApplication>
#include <QMessageBox>

QString restart_required_setting_label(config::RestartRequiredSetting setting) {
    switch (setting) {
    case config::RestartRequiredSetting::CpuOpt:
        return QCoreApplication::translate("SettingsDialogTooltips", "Enable CPU Optimizations");
    case config::RestartRequiredSetting::BackendRenderer:
        return QCoreApplication::translate("SettingsDialogTooltips", "Backend Renderer");
    case config::RestartRequiredSetting::GraphicsDevice:
        return QCoreApplication::translate("SettingsDialogTooltips", "Graphics Device");
    case config::RestartRequiredSetting::CustomDriver:
        return QCoreApplication::translate("SettingsDialogTooltips", "Custom Driver");
    case config::RestartRequiredSetting::HighAccuracy:
        return QCoreApplication::translate("SettingsDialogTooltips", "Rendering Accuracy");
    case config::RestartRequiredSetting::ResolutionMultiplier:
        return QCoreApplication::translate("SettingsDialogTooltips", "Internal Resolution Upscaling");
    case config::RestartRequiredSetting::MemoryMapping:
        return QCoreApplication::translate("SettingsDialogTooltips", "Memory Mapping");
    case config::RestartRequiredSetting::AudioBackend:
        return QCoreApplication::translate("SettingsDialogTooltips", "Audio Backend");
    case config::RestartRequiredSetting::ValidationLayer:
        return QCoreApplication::translate("SettingsDialogTooltips", "Vulkan Validation Layer");
    }

    return {};
}

bool prompt_restart_required_dialog(QWidget *parent, const std::vector<config::RestartRequiredSetting> &settings) {
    if (settings.empty())
        return false;

    QStringList changed_settings;
    for (const auto setting : settings)
        changed_settings << restart_required_setting_label(setting);

    const auto result = gui::utils::show_message_box(
        parent,
        QMessageBox::Information,
        QCoreApplication::translate("SettingsDialogTooltips", "Restart Required"),
        QCoreApplication::translate("SettingsDialogTooltips", "Some changes need an app restart to fully take effect."),
        {
            { QStringLiteral("restart"), QCoreApplication::translate("SettingsDialogTooltips", "Restart App"), QMessageBox::AcceptRole, true },
            { QStringLiteral("later"), QCoreApplication::translate("SettingsDialogTooltips", "Restart Later"), QMessageBox::RejectRole, false },
        },
        QCoreApplication::translate(
            "SettingsDialogTooltips",
            "These changed settings will apply after restart:\n- %1")
            .arg(changed_settings.join(QStringLiteral("\n- "))));

    return result.clicked_id == QStringLiteral("restart");
}

QString SettingsDialogTooltips::category_summary(SettingsTab tab) const {
    switch (tab) {
    case SettingsTab::Core:
        return tr("Control how low-level modules are loaded.");
    case SettingsTab::CPU:
        return tr("Manage CPU behavior.");
    case SettingsTab::Graphics:
        return tr("Tune renderer behavior, textures, and shader behavior.");
    case SettingsTab::Audio:
        return tr("Adjust audio.");
    case SettingsTab::Camera:
        return tr("Choose how front and back camera input should be simulated.");
    case SettingsTab::System:
        return tr("Set regional behavior, and emulated console system related settings here.");
    case SettingsTab::Emulator:
        return tr("Configure app-level behavior in this section.");
    case SettingsTab::Interface:
        return tr("Customize the Vita3K interface.");
    case SettingsTab::Network:
        return tr("Configure network related settings here.");
    case SettingsTab::Debug:
        return tr("Internal diagnostics, GPU debugging, and watch controls live here. These settings are mainly for developers.");
    default:
        return default_description;
    }
}

SettingsDialogTooltips::SettingsDialogTooltips(QObject *parent)
    : QObject(parent)
    , default_description(tr("Point your mouse at an option to display a description here."))
    , modules_automatic(tr("Automatically select modules to load. Recommended for most apps."))
    , modules_auto_manual(tr("Select this mode to load modules automatically in addition to selected modules from the list on the right."))
    , modules_manual(tr("Only load the modules selected from the list. Advanced users only."))
    , cpu_opt(tr("Enable Dynarmic JIT optimizations. Improves performance."))
    , backend_renderer(tr("Select the preferred backend renderer. Vulkan is recommended for most systems."))
    , renderer_accuracy(tr("Set the renderer accuracy level for Vulkan. High accuracy may improve visuals but reduce performance."))
    , vsync(tr("Enable V-Sync for OpenGL. Reduces screen tearing."))
    , disable_surface_sync(tr("Speed hack, disabling turns off surface syncing between CPU and GPU. Surface syncing is needed by a few games.\nGives a big performance boost if disabled (in particular when upscaling is on)."))
    , async_pipeline(tr("Allow pipelines to be compiled concurrently on multiple concurrent threads.\nThis decreases pipeline compilation stutter at the cost of temporary graphical glitches."))
    , screen_filter(tr("Select the final image filter to apply."))
    , gpu_device(tr("Select which GPU Vita3k should use."))
    , resolution_upscaling(tr("Scale the games resolution by a multiplier.\nExperimental: apps are not guaranteed to render properly at more than 1x."))
    , anisotropic_filtering(tr("Technique to increase the sharpness of textures which are sloped relative to the viewer.\nIt has no drawbacks but can impact performance on older GPUs."))
    , export_textures(tr("Export textures used by the app to the textures folder."))
    , import_textures(tr("Import replacement textures from the textures folder."))
    , texture_export_format(tr("Format to export textures in."))
    , shader_cache(tr("Enable shader caching. Reduces stuttering on subsequent runs."))
    , spirv_shader(tr("Pass generated Spir-V shader directly to driver.\nNote that some beneficial extensions will be disabled, and not all GPUs are compatible with this."))
    , fps_hack(tr("Game hack. May double the framerate from 30 FPS to 60 FPS in some games, but can cause some games to run twice as fast."))
    , memory_mapping(tr("Memory mapping improved performance, reduces memory usage and fixes many graphical issues. However, it may be unstable on some GPUs."))
    , audio_backend(tr("Select the audio backend. Cubeb is recommended for most systems."))
    , audio_volume(tr("Set the in-game audio volume."))
    , theme_music_enable(tr("Enable background music for generated Vita themes when the selected theme provides an ATRAC9 BGM file."))
    , theme_music_volume(tr("Set the playback volume for Vita theme background music. This only affects interface theme music."))
    , ngs_enable(tr("Enable advanced audio library NGS support."))
    , front_camera(tr("Select the front camera source.\nSolid Color or Static Image can be used as substitutes."))
    , back_camera(tr("Select the back camera source.\nSolid Color or Static Image can be used as substitutes."))
    , enter_button(tr("Select which button acts as the Enter/Confirm button. Some apps ignore this setting."))
    , pstv_mode(tr("Enable PlayStation TV mode."))
    , show_mode(tr("Enable Show Mode."))
    , demo_mode(tr("Enable Demo Mode."))
    , boot_fullscreen(tr("Automatically enter fullscreen when booting an app."))
    , show_live_area(tr("Show an imitated PS Vita Live Area screen before booting an app."))
    , show_compile_shaders(tr("Show a hint when shaders are being compiled during gameplay."))
    , check_for_updates(tr("Choose whether Vita3K checks for updates at startup.\n\"Check\" notifies you when a newer build is available, and \"Don't Check\" disables startup checks."))
    , log_compat_warn(tr("Log compatibility database related warnings."))
    , texture_cache(tr("Enable the texture cache. Improves performance in games at the cost of additional VRAM usage."))
    , archive_log(tr("Keep a duplicate of the log file named after the current title when an app runs."))
    , log_level(tr("Select the verbosity of the logging system."))
    , discord_rpc(tr("Enable Discord Rich Presence to show what app you're running on Discord."))
    , perf_overlay(tr("Show a performance overlay with FPS and other statistics."))
    , perf_overlay_detail(tr("Select the detail level for the performance overlay."))
    , perf_overlay_position(tr("Select the location to display the performance overlay in."))
    , file_loading_delay(tr("Add an artificial delay to file loading. This is required for some games that load files too quickly compared to real hardware (e.g., Silent Hill)."))
    , stretch_display(tr("Stretch the display area to fill the entire window, ignoring the original aspect ratio."))
    , pixel_perfect(tr("Check the box to get a pixel perfect rendering with HD resolutions (1080p, 4K) in fullscreen on a 16:9 aspect ratio monitor at the price of slight cropping at the top and the bottom of the screen."))
    , emulator_storage_folder(tr("Change the location of the emulated system storage folder.\nIf you move to a different folder, move your existing data there manually."))
    , custom_config(tr("This will remove all per-app custom configuration files. This action cannot be undone."))
    , screenshot_format(tr("Select image format to select screenshots in."))
    , psn_signed_in(tr("Pretend to be signed in to PlayStation Network (but offline)."))
    , http_enable(tr("Enable HTTP networking support for apps that use it."))
    , http_timeout_attempts(tr("How many times to retry connecting when the server doesn't respond.  Could be useful if you have very unstable or VERY SLOW internet."))
    , http_timeout_sleep(tr("How long to wait between connection retries. Could be useful if you have very unstable or VERY SLOW internet."))
    , http_read_end_attempts(tr("How many retries to attempt when there isn't more data to read, lower can improve performance but can make games unstable if you have bad enough internet."))
    , http_read_end_sleep(tr("How long to wait between read attempt retries, lower can improve performance but can make games unstable if you have bad enough internet"))
    , adhoc_address(tr("Select the IP address to be used in adhoc networking."))
    , log_imports(tr("Log module import symbols."))
    , log_exports(tr("Log module export symbols."))
    , log_active_shaders(tr("Log active shader programs."))
    , log_uniforms(tr("Log shader uniform values."))
    , color_surface_debug(tr("Save color surfaces to files."))
    , validation_layer(tr("Enable Vulkan validation layers. Useful for debugging but reduces performance."))
    , dump_elfs(tr("Dump loaded code as ELFs."))
    , sys_lang(tr("Set the system language reported to applications."))
    , sys_date_format(tr("Set the date format used by the emulated system."))
    , sys_time_format(tr("Set the time format used by the emulated system."))
    , ime_langs(tr("Select which IME keyboard languages are available to applications."))
    , show_welcome_screen(tr("Show the Welcome Dialog at startup."))
    , warn_missing_firmware(tr("Show a warning before launching a game when one or more firmware packages are missing."))
    , confirm_exit_app(tr("Show a confirmation dialog before closing a running app."))
    , warn_admin_privileges(tr("Show a warning at startup when Vita3K is launched with administrator or root privileges."))
    , windows_rounded_corners(tr("Enable rounded corners for the game window on Windows.\nUnsupported Windows versions may ignore this setting."))
    , log_buffer_size(tr("Set the maximum number of log lines to keep in memory.\nSet to 0 to remove the user cap and use the built-in safety limit."))
    , log_font(tr("Choose the font family used by the log view."))
    , gui_stylesheet(tr("Select the visual theme for the application.\nYou can also place custom .qss files in the gui-configs folder."))
    , ui_language(tr("Select the desktop interface language used by the Qt frontend.\nUse System Default to follow the operating system language.")) {
}
