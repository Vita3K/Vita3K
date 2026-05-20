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

#pragma once

#include <config/settings.h>

#include <QObject>
#include <QString>

enum class SettingsTab : int;
class QWidget;

QString restart_required_setting_label(config::RestartRequiredSetting setting);
bool prompt_restart_required_dialog(QWidget *parent, const std::vector<config::RestartRequiredSetting> &settings);

class SettingsDialogTooltips : public QObject {
    Q_OBJECT

public:
    explicit SettingsDialogTooltips(QObject *parent = nullptr);

    QString category_summary(SettingsTab tab) const;

    QString default_description;

    QString modules_automatic;
    QString modules_auto_manual;
    QString modules_manual;
    QString cpu_opt;
    QString backend_renderer;
    QString renderer_accuracy;
    QString vsync;
    QString disable_surface_sync;
    QString async_pipeline;
    QString screen_filter;
    QString gpu_device;
    QString resolution_upscaling;
    QString anisotropic_filtering;
    QString export_textures;
    QString import_textures;
    QString texture_export_format;
    QString shader_cache;
    QString spirv_shader;
    QString fps_hack;
    QString memory_mapping;
    QString audio_backend;
    QString audio_volume;
    QString theme_music_enable;
    QString theme_music_volume;
    QString ngs_enable;
    QString front_camera;
    QString back_camera;
    QString enter_button;
    QString pstv_mode;
    QString show_mode;
    QString demo_mode;
    QString boot_fullscreen;
    QString show_live_area;
    QString show_compile_shaders;
    QString check_for_updates;
    QString log_compat_warn;
    QString texture_cache;
    QString archive_log;
    QString log_level;
    QString discord_rpc;
    QString perf_overlay;
    QString perf_overlay_detail;
    QString perf_overlay_position;
    QString file_loading_delay;
    QString stretch_display;
    QString pixel_perfect;
    QString emulator_storage_folder;
    QString custom_config;
    QString screenshot_format;
    QString psn_signed_in;
    QString http_enable;
    QString http_timeout_attempts;
    QString http_timeout_sleep;
    QString http_read_end_attempts;
    QString http_read_end_sleep;
    QString adhoc_address;
    QString log_imports;
    QString log_exports;
    QString log_active_shaders;
    QString log_uniforms;
    QString color_surface_debug;
    QString validation_layer;
    QString dump_elfs;
    QString sys_lang;
    QString sys_date_format;
    QString sys_time_format;
    QString ime_langs;
    QString show_welcome_screen;
    QString warn_missing_firmware;
    QString confirm_exit_app;
    QString warn_admin_privileges;
    QString windows_rounded_corners;
    QString log_buffer_size;
    QString log_font;
    QString gui_stylesheet;
    QString ui_language;
};
