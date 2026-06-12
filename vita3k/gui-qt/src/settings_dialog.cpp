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

#include "ui_settings_dialog.h"

#include <app/functions.h>
#include <app/state.h>
#include <audio/state.h>
#include <compat/state.h>
#include <config/functions.h>
#include <config/settings.h>
#include <emuenv/state.h>
#include <gui-qt/gui_language.h>
#include <gui-qt/gui_settings.h>
#include <gui-qt/qt_utils.h>
#include <gui-qt/settings_dialog.h>
#include <gui-qt/settings_dialog_tooltips.h>
#include <gui-qt/theme_manager.h>
#include <io/state.h>
#include <kernel/state.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <util/log.h>
#include <util/system.h>

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QTabBar>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QtGlobal>

#include <algorithm>

namespace {

int first_visible_category(const QListWidget *list, int preferred) {
    const int count = list->count();
    if (preferred >= 0 && preferred < count) {
        if (const auto *item = list->item(preferred); item && !item->isHidden())
            return preferred;
    }

    for (int i = 0; i < count; ++i) {
        if (const auto *item = list->item(i); item && !item->isHidden())
            return i;
    }

    return -1;
}

} // namespace

SettingsDialog::SettingsDialog(EmuEnvState &emuenv,
    std::shared_ptr<GuiSettings> gui_settings,
    ThemeManager &theme_manager,
    const std::string &app_path,
    int initial_tab)
    : QDialog(nullptr)
    , emuenv(emuenv)
    , m_gui_settings(std::move(gui_settings))
    , m_theme_manager(theme_manager)
    , m_tooltips(std::make_unique<SettingsDialogTooltips>(this))
    , m_app_path(app_path)
    , m_initial_tab(initial_tab)
    , m_ui(std::make_unique<Ui::vita3k_settings_dialog>()) {
    m_ui->setupUi(this);
    setWindowModality(Qt::NonModal);
    setWindowFlag(Qt::Window, true);
    setWindowFlag(Qt::WindowSystemMenuHint, true);
    setWindowFlag(Qt::WindowMinMaxButtonsHint, true);
    setWindowFlag(Qt::WindowCloseButtonHint, true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(QStringLiteral(":/Vita3K.png")));
    m_ui->tab_widget_settings->tabBar()->hide();
    m_ui->log_font_family->setEditable(true);
    m_ui->log_font_family->setInsertPolicy(QComboBox::NoInsert);
    if (auto *font_edit = m_ui->log_font_family->lineEdit()) {
        font_edit->setClearButtonEnabled(true);
        font_edit->setPlaceholderText(tr("Search fonts..."));
    }
    if (auto *font_completer = m_ui->log_font_family->completer()) {
        font_completer->setCompletionMode(QCompleter::PopupCompletion);
        font_completer->setFilterMode(Qt::MatchContains);
        font_completer->setCaseSensitivity(Qt::CaseInsensitive);
    }
    if (!m_gui_settings || !restoreGeometry(m_gui_settings->get_value(gui::sd_geometry).toByteArray()))
        resize(780, 676);
    connect(&m_theme_manager, &ThemeManager::theme_state_changed, this, &SettingsDialog::refresh_themes);
    connect(&m_theme_manager, &ThemeManager::theme_catalog_changed, this, &SettingsDialog::refresh_themes);
    m_ui->settingsCategory->setSpacing(0);
    m_ui->modules_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_ui->modules_list->setFocusPolicy(Qt::NoFocus);
    m_ui->modules_list->setAlternatingRowColors(true);
    m_ui->tracy_modules_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_ui->tracy_modules_list->setFocusPolicy(Qt::NoFocus);
    m_ui->tracy_modules_list->setAlternatingRowColors(true);

    for (int i = 0; i < m_ui->tab_widget_settings->count(); ++i) {
        auto *item = new QListWidgetItem(m_ui->tab_widget_settings->tabText(i), m_ui->settingsCategory);
        item->setSizeHint(QSize(0, 34));
    }

    m_button_boxes = { m_ui->button_box_main };
    apply_text_overrides();

    if (!m_app_path.empty()) {
        QString title_name = QString::fromStdString(m_app_path);
        for (const auto &app : emuenv.app.apps_list.apps) {
            if (app.title_id == m_app_path) {
                title_name = QString::fromStdString(app.title);
                break;
            }
        }
        setWindowTitle(tr("Settings - %1").arg(title_name));
    }

#ifndef _WIN32
    m_ui->windows_rounded_corners->setVisible(false);
#endif

    load_config();
    setup_connections();
    update_gpu_visibility();
    update_camera_widgets();
    setup_dirty_tracking();

    if (!m_app_path.empty()) {
        auto *tabs = m_ui->tab_widget_settings;
        for (int i = tabs->count() - 1; i >= 0; --i) {
            QWidget *page = tabs->widget(i);
            if (page == m_ui->camera_tab
                || page == m_ui->gui_tab) {
                tabs->setTabVisible(i, false);
                if (auto *item = m_ui->settingsCategory->item(i))
                    item->setHidden(true);
            }
        }

        m_ui->show_mode->setVisible(false);
        m_ui->demo_mode->setVisible(false);

        m_ui->gb_theme_music_section->setVisible(false);
        m_ui->boot_apps_fullscreen->setVisible(false);
        m_ui->show_live_area_screen->setVisible(false);
        m_ui->show_compile_shaders->setVisible(false);
        m_ui->label_check_for_updates_mode->setVisible(false);
        m_ui->check_for_updates_mode->setVisible(false);
        m_ui->log_compat_warn->setVisible(false);
        m_ui->archive_log->setVisible(false);
        m_ui->discord_rich_presence->setVisible(false);
        m_ui->gb_emu_logging->setVisible(false);
        m_ui->gb_performance_overlay->setVisible(false);
        m_ui->gb_storage->setVisible(false);
        m_ui->gb_custom_config->setVisible(false);
        m_ui->gb_screenshot->setVisible(false);

        m_ui->gb_network_http_section->setVisible(false);
        m_ui->gb_adhoc->setVisible(false);
    }

    const int selected_tab = first_visible_category(m_ui->settingsCategory, m_initial_tab);
    if (selected_tab >= 0) {
        m_ui->settingsCategory->setCurrentRow(selected_tab);
        m_ui->tab_widget_settings->setCurrentIndex(selected_tab);
        set_description(m_ui->tab_widget_settings->widget(selected_tab), {}, m_tooltips->default_description);
    }

    for (auto *bb : m_button_boxes)
        bb->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void SettingsDialog::refresh_themes() {
    const QString preferred_name = m_ui->combo_stylesheets
        ? m_ui->combo_stylesheets->currentData().toString()
        : m_current_stylesheet;
    add_stylesheets(preferred_name);
}

SettingsDialog::~SettingsDialog() {
}

void SettingsDialog::set_storage_path_locked(bool locked) {
    m_storage_path_locked = locked;
    if (m_app_path.empty())
        m_ui->gb_storage->setEnabled(!locked);
}

#ifdef TRACY_ENABLE
void SettingsDialog::populate_tracy_modules_list() {
    m_ui->tracy_modules_list->clear();
    const auto enabled_modules = m_config.tracy_advanced_profiling_modules;
    for (const auto &name : tracy_module_utils::get_available_module_names()) {
        const bool enabled = std::ranges::contains(enabled_modules.begin(), enabled_modules.end(), name);
        auto *item = new QListWidgetItem(QString::fromStdString(name), m_ui->tracy_modules_list);
        item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsSelectable);
        item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
}
#endif

void SettingsDialog::closeEvent(QCloseEvent *event) {
    if (m_gui_settings) {
        m_gui_settings->set_value(gui::sd_geometry, saveGeometry(), false);
        m_gui_settings->set_value(gui::sd_lastTab, m_ui->settingsCategory->currentRow(), false);
        m_gui_settings->sync();
    }

    QDialog::closeEvent(event);
}

void SettingsDialog::done(int result) {
    if (m_gui_settings) {
        m_gui_settings->set_value(gui::sd_geometry, saveGeometry(), false);
        m_gui_settings->set_value(gui::sd_lastTab, m_ui->settingsCategory->currentRow(), false);
        m_gui_settings->sync();
    }

    QDialog::done(result);
}

void SettingsDialog::show_tab(int tab_index) {
    const int selected_tab = first_visible_category(m_ui->settingsCategory, tab_index);
    if (selected_tab < 0)
        return;

    m_ui->settingsCategory->setCurrentRow(selected_tab);
    m_ui->tab_widget_settings->setCurrentIndex(selected_tab);
    set_description(m_ui->tab_widget_settings->widget(selected_tab), {}, m_tooltips->default_description);
}

void SettingsDialog::apply_text_overrides() {
    m_ui->helpText->setOpenExternalLinks(true);
    m_ui->helpText->document()->setDocumentMargin(0);
}

void SettingsDialog::load_config() {
    Config cfg_copy;
    cfg_copy = emuenv.cfg;
    config::set_current_config(cfg_copy, emuenv.config_path, m_app_path);
    m_config = cfg_copy.current_config;

    switch (m_config.modules_mode) {
    case ModulesMode::AUTOMATIC:
        m_ui->rb_modules_automatic->setChecked(true);
        break;
    case ModulesMode::AUTO_MANUAL:
        m_ui->rb_modules_auto_manual->setChecked(true);
        break;
    case ModulesMode::MANUAL:
        m_ui->rb_modules_manual->setChecked(true);
        break;
    }
    populate_modules_list();

    m_ui->cpu_opt->setChecked(m_config.cpu_opt);

    m_ui->backend_renderer_box->clear();
#ifndef __APPLE__
    m_ui->backend_renderer_box->addItem(QStringLiteral("OpenGL"));
#endif
    m_ui->backend_renderer_box->addItem(QStringLiteral("Vulkan"));
    {
        const int idx = m_ui->backend_renderer_box->findText(
            QString::fromStdString(m_config.backend_renderer));
        if (idx >= 0)
            m_ui->backend_renderer_box->setCurrentIndex(idx);
    }

    m_ui->gpu_device_box->clear();
    if (emuenv.vulkan_device_info) {
        for (const auto &gpu : emuenv.vulkan_device_info->gpu_names)
            m_ui->gpu_device_box->addItem(QString::fromStdString(gpu));
    }

    {
        const int gpu_count = m_ui->gpu_device_box->count();
        const int idx = (m_config.gpu_idx >= 0 && m_config.gpu_idx < gpu_count)
            ? m_config.gpu_idx
            : 0;
        m_ui->gpu_device_box->setCurrentIndex(idx);
    }

    m_ui->renderer_accuracy_box->clear();
    m_ui->renderer_accuracy_box->addItem(tr("Standard"));
    m_ui->renderer_accuracy_box->addItem(tr("High"));
    m_ui->renderer_accuracy_box->setCurrentIndex(m_config.high_accuracy ? 1 : 0);

    m_ui->vsync->setChecked(m_config.v_sync);
    m_ui->disable_surface_sync->setChecked(m_config.disable_surface_sync);
    m_ui->async_pipeline_compilation->setChecked(m_config.async_pipeline_compilation);

    {
        m_ui->memory_mapping_box->clear();

        struct MappingEntry {
            const char *label;
            const char *value;
            int bit;
        };
        static const MappingEntry all_methods[] = {
            { "Disabled", "disabled", static_cast<int>(MappingMethod::Disabled) },
            { "Double Buffer", "double-buffer", static_cast<int>(MappingMethod::DoubleBuffer) },
            { "External Host", "external-host", static_cast<int>(MappingMethod::ExernalHost) },
            { "Page Table", "page-table", static_cast<int>(MappingMethod::PageTable) },
            { "Native Buffer", "native-buffer", static_cast<int>(MappingMethod::NativeBuffer) },
        };

        const int gpu_idx = m_config.gpu_idx;
        const int mask = app::get_supported_memory_mapping_mask(emuenv, gpu_idx);

        int current_idx = 0;
        int added = 0;
        for (const auto &m : all_methods) {
            if ((1 << m.bit) & mask) {
                m_ui->memory_mapping_box->addItem(tr(m.label), QString::fromLatin1(m.value));
                if (m_config.memory_mapping == m.value)
                    current_idx = added;
                added++;
            }
        }
        m_ui->memory_mapping_box->setCurrentIndex(current_idx);
    }

    m_ui->resolution_upscale->setMinimum(2);
    m_ui->resolution_upscale->setMaximum(32);
    m_ui->resolution_upscale->setSingleStep(1);
    m_ui->resolution_upscale->setValue(static_cast<int>(m_config.resolution_multiplier * 4.0f));
    update_resolution_label();

    m_ui->anisotropic_filter->setMinimum(0);
    m_ui->anisotropic_filter->setMaximum(4);
    m_ui->anisotropic_filter->setSingleStep(1);
    m_ui->anisotropic_filter->setValue(
        static_cast<int>(log2f(static_cast<float>(m_config.anisotropic_filtering))));
    update_aniso_label();

    m_ui->export_textures->setChecked(m_config.export_textures);
    m_ui->import_textures->setChecked(m_config.import_textures);

    m_ui->texture_export_format->clear();
    m_ui->texture_export_format->addItems({ QStringLiteral("PNG"), QStringLiteral("DDS") });
    m_ui->texture_export_format->setCurrentIndex(m_config.export_as_png ? 0 : 1);

    m_ui->shader_cache->setChecked(m_config.shader_cache);
    m_ui->spirv_shader->setChecked(m_config.spirv_shader);
    m_ui->fps_hack->setChecked(m_config.fps_hack);

    m_ui->audio_backend_box->clear();
    m_ui->audio_backend_box->addItems({ QStringLiteral("SDL"), QStringLiteral("Cubeb") });
    m_ui->audio_backend_box->setCurrentIndex(m_config.audio_backend == "Cubeb" ? 1 : 0);

    m_ui->audio_volume->setValue(m_config.audio_volume);
    m_ui->audio_volume_label->setText(tr("Current volume: %1%").arg(m_config.audio_volume));
    const bool theme_music_enabled = m_gui_settings
        ? m_gui_settings->get_value(gui::vt_bgmEnabled).toBool()
        : gui::vt_bgmEnabled.def.toBool();
    const int theme_music_volume = std::clamp(
        m_gui_settings
            ? m_gui_settings->get_value(gui::vt_bgmVolume).toInt()
            : gui::vt_bgmVolume.def.toInt(),
        0, 100);
    m_ui->theme_music_enable->setChecked(theme_music_enabled);
    m_ui->theme_music_volume->setValue(theme_music_volume);
    m_ui->theme_music_volume_label->setText(tr("Theme music volume: %1%").arg(theme_music_volume));

    m_ui->ngs_enable->setChecked(m_config.ngs_enable);

    app::ensure_camera_defaults(emuenv.cfg);
    populate_cameras_list();

    if (m_config.sys_button == 0)
        m_ui->enter_button_circle->setChecked(true);
    else
        m_ui->enter_button_cross->setChecked(true);

    m_ui->pstv_mode->setChecked(m_config.pstv_mode);
    m_ui->show_mode->setChecked(emuenv.cfg.show_mode);
    m_ui->demo_mode->setChecked(emuenv.cfg.demo_mode);

    m_ui->boot_apps_fullscreen->setChecked(emuenv.cfg.boot_apps_full_screen);
    m_ui->show_live_area_screen->setChecked(emuenv.cfg.show_live_area_screen);
    m_ui->show_compile_shaders->setChecked(emuenv.cfg.show_compile_shaders);
    m_ui->check_for_updates_mode->clear();
    m_ui->check_for_updates_mode->addItem(tr("Check"), static_cast<int>(UPDATE_STARTUP_PROMPT));
    m_ui->check_for_updates_mode->addItem(tr("Don't Check"), static_cast<int>(UPDATE_STARTUP_OFF));
    {
        const int current_mode = emuenv.cfg.check_for_updates_mode == static_cast<int>(UPDATE_STARTUP_OFF)
            ? static_cast<int>(UPDATE_STARTUP_OFF)
            : static_cast<int>(UPDATE_STARTUP_PROMPT);
        const int index = m_ui->check_for_updates_mode->findData(current_mode);
        m_ui->check_for_updates_mode->setCurrentIndex(index >= 0 ? index : 0);
    }
    m_ui->log_compat_warn->setChecked(emuenv.cfg.log_compat_warn);
    m_ui->texture_cache->setChecked(m_config.texture_cache);
    m_ui->archive_log->setChecked(emuenv.cfg.archive_log);
#ifdef USE_DISCORD
    m_ui->discord_rich_presence->setChecked(emuenv.cfg.discord_rich_presence);
#else
    m_ui->discord_rich_presence->setVisible(false);
#endif

    m_ui->log_level_box->clear();
    m_ui->log_level_box->addItems({ tr("Trace"), tr("Debug"), tr("Info"),
        tr("Warning"), tr("Error"), tr("Critical"), tr("Off") });
    m_ui->log_level_box->setCurrentIndex(emuenv.cfg.log_level);

    m_ui->perf_overlay_enabled->setChecked(emuenv.cfg.performance_overlay);
    m_ui->perf_overlay_detail_box->clear();
    m_ui->perf_overlay_detail_box->addItems({ tr("Minimum"), tr("Low"), tr("Medium"), tr("Maximum") });
    m_ui->perf_overlay_detail_box->setCurrentIndex(emuenv.cfg.performance_overlay_detail);
    m_ui->perf_overlay_position_box->clear();
    m_ui->perf_overlay_position_box->addItems({ tr("Top Left"), tr("Top Center"), tr("Top Right"),
        tr("Bottom Left"), tr("Bottom Center"), tr("Bottom Right") });
    m_ui->perf_overlay_position_box->setCurrentIndex(emuenv.cfg.performance_overlay_position);

    m_ui->label_perf_detail->setEnabled(emuenv.cfg.performance_overlay);
    m_ui->perf_overlay_detail_box->setEnabled(emuenv.cfg.performance_overlay);
    m_ui->label_perf_position->setEnabled(emuenv.cfg.performance_overlay);
    m_ui->perf_overlay_position_box->setEnabled(emuenv.cfg.performance_overlay);

    m_pending_vita_fs_path = emuenv.vita_fs_path;
    update_current_emu_path_label();

    m_ui->screenshot_format->clear();
    m_ui->screenshot_format->addItems({ tr("None"), QStringLiteral("JPEG"), QStringLiteral("PNG") });
    m_ui->screenshot_format->setCurrentIndex(emuenv.cfg.screenshot_format);

    m_ui->stretch_the_display_area->setChecked(m_config.stretch_the_display_area);
    m_ui->fullscreen_hd_res_pixel_perfect->setChecked(m_config.fullscreen_hd_res_pixel_perfect);

    m_ui->file_loading_delay->setMinimum(0);
    m_ui->file_loading_delay->setMaximum(30);
    m_ui->file_loading_delay->setValue(m_config.file_loading_delay);
    update_file_loading_delay_label();

    m_ui->sys_lang_box->clear();
    m_ui->sys_lang_box->addItems({ tr("Japanese"), tr("English (US)"), tr("French"), tr("Spanish"),
        tr("German"), tr("Italian"), tr("Dutch"), tr("Portuguese (PT)"),
        tr("Russian"), tr("Korean"), tr("Chinese (Traditional)"),
        tr("Chinese (Simplified)"), tr("Finnish"), tr("Swedish"),
        tr("Danish"), tr("Norwegian"), tr("Polish"), tr("Portuguese (BR)"),
        tr("English (GB)"), tr("Turkish") });
    m_ui->sys_lang_box->setCurrentIndex(m_config.sys_lang);

    m_ui->sys_date_format_box->clear();
    m_ui->sys_date_format_box->addItems({ tr("YYYY/MM/DD"), tr("DD/MM/YYYY"), tr("MM/DD/YYYY") });
    m_ui->sys_date_format_box->setCurrentIndex(m_config.sys_date_format);

    m_ui->sys_time_format_box->clear();
    m_ui->sys_time_format_box->addItems({ tr("12-Hour"), tr("24-Hour") });
    m_ui->sys_time_format_box->setCurrentIndex(m_config.sys_time_format);

    {
        struct ImeLang {
            const char *name;
            uint64_t flag;
        };
        static const ImeLang ime_lang_list[] = {
            { "Danish", 0x00000001ULL },
            { "German", 0x00000002ULL },
            { "English (US)", 0x00000004ULL },
            { "Spanish", 0x00000008ULL },
            { "French", 0x00000010ULL },
            { "Italian", 0x00000020ULL },
            { "Dutch", 0x00000040ULL },
            { "Norwegian", 0x00000080ULL },
            { "Polish", 0x00000100ULL },
            { "Portuguese (PT)", 0x00000200ULL },
            { "Russian", 0x00000400ULL },
            { "Finnish", 0x00000800ULL },
            { "Swedish", 0x00001000ULL },
            { "Japanese", 0x00002000ULL },
            { "Korean", 0x00004000ULL },
            { "Simplified Chinese", 0x00008000ULL },
            { "Traditional Chinese", 0x00010000ULL },
            { "Portuguese (BR)", 0x00020000ULL },
            { "English (GB)", 0x00040000ULL },
            { "Turkish", 0x00080000ULL },
        };
        m_ui->ime_langs_list->clear();

        uint64_t enabled_mask = 0;
        for (const auto &v : m_config.ime_langs)
            enabled_mask |= v;
        for (const auto &lang : ime_lang_list) {
            auto *item = new QListWidgetItem(tr(lang.name), m_ui->ime_langs_list);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState((enabled_mask & lang.flag) ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, QVariant::fromValue(lang.flag));
        }
    }

    m_ui->psn_signed_in->setChecked(m_config.psn_signed_in);
    m_ui->http_enable->setChecked(emuenv.cfg.http_enable);
    m_ui->http_timeout_attempts->setValue(emuenv.cfg.http_timeout_attempts);
    m_ui->http_timeout_sleep->setValue(emuenv.cfg.http_timeout_sleep_ms);
    m_ui->http_read_end_attempts->setValue(emuenv.cfg.http_read_end_attempts);
    m_ui->http_read_end_sleep->setValue(emuenv.cfg.http_read_end_sleep_ms);
    update_http_retry_labels();
    populate_adhoc_list();

    m_ui->show_welcome->setChecked(emuenv.cfg.show_welcome);
    m_ui->warn_missing_firmware->setChecked(emuenv.cfg.warn_missing_firmware);
    m_ui->confirm_exit_app->setChecked(m_gui_settings
            ? m_gui_settings->get_value(gui::mw_confirmExitApp).toBool()
            : true);
    m_ui->warn_admin_privileges->setChecked(m_gui_settings
            ? m_gui_settings->get_value(gui::mw_warnAdminPrivileges).toBool()
            : true);
    m_ui->windows_rounded_corners->setChecked(m_gui_settings
            ? m_gui_settings->get_value(gui::gw_roundedCorners).toBool()
            : false);
    m_ui->ui_language_box->clear();
    m_ui->ui_language_box->addItem(tr("System Default"), QStringLiteral(""));

    for (const auto &language : gui::i18n::ui_language_options(emuenv.static_assets_path)) {
        const QString tag = QString::fromUtf8(language.tag.data(), static_cast<int>(language.tag.size()));
        const QString native_name = QString::fromUtf8(language.native_name.data(), static_cast<int>(language.native_name.size()));
        m_ui->ui_language_box->addItem(native_name, tag);
    }
    {
        const QString current_lang = QString::fromStdString(emuenv.cfg.user_lang);
        const int index = m_ui->ui_language_box->findData(current_lang);
        m_ui->ui_language_box->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (m_gui_settings) {
        const int buf_size = m_gui_settings->get_value(gui::l_bufferSize).toInt();
        m_ui->log_buffer_size->setValue(buf_size);
        const QString log_font_family = m_gui_settings->get_value(gui::l_fontFamily).toString();
        m_ui->log_font_family->setCurrentFont(log_font_family.isEmpty()
                ? QFontDatabase::systemFont(QFontDatabase::FixedFont)
                : QFont(log_font_family));
    }

    add_stylesheets();

    m_ui->log_imports->setChecked(emuenv.kernel.debugger.log_imports);
    m_ui->log_exports->setChecked(emuenv.kernel.debugger.log_exports);
    m_ui->log_active_shaders->setChecked(m_config.log_active_shaders);
    m_ui->log_uniforms->setChecked(m_config.log_uniforms);
    m_ui->color_surface_debug->setChecked(m_config.color_surface_debug);
    m_ui->validation_layer->setChecked(m_config.validation_layer);
    m_ui->dump_elfs->setChecked(emuenv.kernel.debugger.dump_elfs);

#ifdef TRACY_ENABLE
    m_ui->gb_debug_tracy_section->setEnabled(true);
    m_ui->tracy_primitive_impl->setChecked(m_config.tracy_primitive_impl);
    populate_tracy_modules_list();
#else
    m_ui->gb_debug_tracy_section->setEnabled(false);
#endif
}

void SettingsDialog::build_desired_config(Config &desired) const {
    desired = emuenv.cfg;
    desired.current_config = emuenv.cfg.current_config;
    if (m_app_path.empty())
        desired.set_vita_fs_path(m_pending_vita_fs_path);
    auto &current = desired.current_config;

    if (m_ui->rb_modules_automatic->isChecked())
        current.modules_mode = ModulesMode::AUTOMATIC;
    else if (m_ui->rb_modules_auto_manual->isChecked())
        current.modules_mode = ModulesMode::AUTO_MANUAL;
    else
        current.modules_mode = ModulesMode::MANUAL;

    current.lle_modules.clear();
    for (int i = 0; i < m_ui->modules_list->count(); ++i) {
        auto *item = m_ui->modules_list->item(i);
        if (item->checkState() == Qt::Checked)
            current.lle_modules.push_back(item->text().toStdString());
    }

    current.cpu_opt = m_ui->cpu_opt->isChecked();
    current.backend_renderer = m_ui->backend_renderer_box->currentText().toStdString();
    current.high_accuracy = m_ui->renderer_accuracy_box->currentIndex() == 1;
    current.v_sync = m_ui->vsync->isChecked();
    current.disable_surface_sync = m_ui->disable_surface_sync->isChecked();
    current.async_pipeline_compilation = m_ui->async_pipeline_compilation->isChecked();
    current.memory_mapping = m_ui->memory_mapping_box->currentData().toString().toStdString();
    current.screen_filter = m_ui->screen_filter_box->currentText().toStdString();
    current.resolution_multiplier = static_cast<float>(m_ui->resolution_upscale->value()) / 4.0f;
    current.anisotropic_filtering = 1 << m_ui->anisotropic_filter->value();
    current.export_textures = m_ui->export_textures->isChecked();
    current.import_textures = m_ui->import_textures->isChecked();
    current.export_as_png = m_ui->texture_export_format->currentIndex() == 0;
    current.fps_hack = m_ui->fps_hack->isChecked();
    current.shader_cache = m_ui->shader_cache->isChecked();
    current.spirv_shader = m_ui->spirv_shader->isChecked();
    current.gpu_idx = m_ui->gpu_device_box->currentIndex();
    current.audio_backend = m_ui->audio_backend_box->currentText().toStdString();
    current.audio_volume = m_ui->audio_volume->value();
    current.ngs_enable = m_ui->ngs_enable->isChecked();

    const int front_idx = m_ui->front_camera_box->currentIndex();
    if (front_idx == 0) {
        desired.front_camera_type = 0;
    } else if (front_idx == 1) {
        desired.front_camera_type = 1;
    } else {
        desired.front_camera_type = 2;
        desired.front_camera_id = m_ui->front_camera_box->currentText().toStdString();
    }

    const int back_idx = m_ui->back_camera_box->currentIndex();
    if (back_idx == 0) {
        desired.back_camera_type = 0;
    } else if (back_idx == 1) {
        desired.back_camera_type = 1;
    } else {
        desired.back_camera_type = 2;
        desired.back_camera_id = m_ui->back_camera_box->currentText().toStdString();
    }

    current.sys_button = m_ui->enter_button_cross->isChecked() ? 1 : 0;
    current.pstv_mode = m_ui->pstv_mode->isChecked();
    desired.show_mode = m_ui->show_mode->isChecked();
    desired.demo_mode = m_ui->demo_mode->isChecked();
    desired.boot_apps_full_screen = m_ui->boot_apps_fullscreen->isChecked();
    desired.show_live_area_screen = m_ui->show_live_area_screen->isChecked();
    desired.show_compile_shaders = m_ui->show_compile_shaders->isChecked();
    desired.check_for_updates_mode = m_ui->check_for_updates_mode->currentData().toInt();
    desired.check_for_updates = desired.check_for_updates_mode != static_cast<int>(UPDATE_STARTUP_OFF);
    desired.log_compat_warn = m_ui->log_compat_warn->isChecked();
    current.texture_cache = m_ui->texture_cache->isChecked();
    desired.archive_log = m_ui->archive_log->isChecked();
#ifdef USE_DISCORD
    desired.discord_rich_presence = m_ui->discord_rich_presence->isChecked();
#endif

    desired.log_level = m_ui->log_level_box->currentIndex();
    desired.performance_overlay = m_ui->perf_overlay_enabled->isChecked();
    desired.performance_overlay_detail = m_ui->perf_overlay_detail_box->currentIndex();
    desired.performance_overlay_position = m_ui->perf_overlay_position_box->currentIndex();
    desired.screenshot_format = m_ui->screenshot_format->currentIndex();
    current.stretch_the_display_area = m_ui->stretch_the_display_area->isChecked();
    current.fullscreen_hd_res_pixel_perfect = m_ui->fullscreen_hd_res_pixel_perfect->isChecked();
    current.file_loading_delay = m_ui->file_loading_delay->value();
    current.sys_lang = m_ui->sys_lang_box->currentIndex();
    current.sys_date_format = m_ui->sys_date_format_box->currentIndex();
    current.sys_time_format = m_ui->sys_time_format_box->currentIndex();

    current.ime_langs.clear();
    for (int i = 0; i < m_ui->ime_langs_list->count(); ++i) {
        auto *item = m_ui->ime_langs_list->item(i);
        if (item->checkState() == Qt::Checked) {
            const uint64_t flag = item->data(Qt::UserRole).toULongLong();
            current.ime_langs.push_back(flag);
        }
    }
    if (current.ime_langs.empty())
        current.ime_langs.push_back(0x00000004ULL); // English (US)

    current.psn_signed_in = m_ui->psn_signed_in->isChecked();
    desired.http_enable = m_ui->http_enable->isChecked();
    desired.http_timeout_attempts = m_ui->http_timeout_attempts->value();
    desired.http_timeout_sleep_ms = m_ui->http_timeout_sleep->value();
    desired.http_read_end_attempts = m_ui->http_read_end_attempts->value();
    desired.http_read_end_sleep_ms = m_ui->http_read_end_sleep->value();
    desired.adhoc_addr = m_ui->adhoc_address_box->currentIndex();
    desired.show_welcome = m_ui->show_welcome->isChecked();
    desired.warn_missing_firmware = m_ui->warn_missing_firmware->isChecked();
    desired.user_lang = m_ui->ui_language_box->currentData().toString().toStdString();
    current.log_active_shaders = m_ui->log_active_shaders->isChecked();
    current.log_uniforms = m_ui->log_uniforms->isChecked();
    current.color_surface_debug = m_ui->color_surface_debug->isChecked();
    current.validation_layer = m_ui->validation_layer->isChecked();

#ifdef TRACY_ENABLE
    current.tracy_primitive_impl = m_ui->tracy_primitive_impl->isChecked();
    current.tracy_advanced_profiling_modules.clear();
    for (int i = 0; i < m_ui->tracy_modules_list->count(); ++i) {
        const auto item = m_ui->tracy_modules_list->item(i);
        const auto name = item->text().toStdString();
        if (item->checkState() == Qt::Checked) {
            tracy_module_utils::set_tracy_active(name, true);
            current.tracy_advanced_profiling_modules.push_back(name);
        } else {
            tracy_module_utils::set_tracy_active(name, false);
        }
    }
#endif
}

bool SettingsDialog::prompt_restart_if_needed(const app::SettingsCommitResult &result, bool close_after) {
    Q_UNUSED(close_after);

    const auto &pending_restart_settings = result.restart_required_settings;
    if (pending_restart_settings.empty()) {
        m_deferred_restart_settings.clear();
        return true;
    }

    const auto has_setting = [](const std::vector<config::RestartRequiredSetting> &settings, config::RestartRequiredSetting setting) {
        return std::find(settings.begin(), settings.end(), setting) != settings.end();
    };

    std::vector<config::RestartRequiredSetting> newly_pending_restart_settings;
    newly_pending_restart_settings.reserve(pending_restart_settings.size());
    for (const auto setting : pending_restart_settings) {
        if (!has_setting(m_deferred_restart_settings, setting))
            newly_pending_restart_settings.push_back(setting);
    }

    m_deferred_restart_settings = pending_restart_settings;

    if (newly_pending_restart_settings.empty())
        return true;

    if (prompt_restart_required_dialog(this, newly_pending_restart_settings)) {
        m_deferred_restart_settings.clear();
        Q_EMIT restart_game_requested();
        return true;
    }

    return true;
}

bool SettingsDialog::commit_changes(bool close_after) {
    const std::string previous_user_lang = emuenv.cfg.user_lang;
    const int previous_log_level = emuenv.cfg.log_level;
    const bool update_gui_settings = m_gui_settings && m_app_path.empty();
    const bool storage_path_changed = m_app_path.empty() && (m_pending_vita_fs_path != emuenv.vita_fs_path);
    const auto previous_buffer_size = update_gui_settings ? m_gui_settings->get_value(gui::l_bufferSize).toInt() : 0;
    const QString previous_log_font_family = update_gui_settings ? m_gui_settings->get_value(gui::l_fontFamily).toString() : QString();
    const bool previous_confirm_exit_app = update_gui_settings
        ? m_gui_settings->get_value(gui::mw_confirmExitApp).toBool()
        : true;
    const bool previous_warn_admin_privileges = update_gui_settings
        ? m_gui_settings->get_value(gui::mw_warnAdminPrivileges).toBool()
        : true;
    const bool previous_windows_rounded_corners = update_gui_settings
        ? m_gui_settings->get_value(gui::gw_roundedCorners).toBool()
        : false;
    const bool previous_theme_music_enabled = update_gui_settings
        ? m_gui_settings->get_value(gui::vt_bgmEnabled).toBool()
        : gui::vt_bgmEnabled.def.toBool();
    const int previous_theme_music_volume = update_gui_settings
        ? m_gui_settings->get_value(gui::vt_bgmVolume).toInt()
        : gui::vt_bgmVolume.def.toInt();

    if (storage_path_changed && !apply_pending_storage_path())
        return false;

    Config desired;
    build_desired_config(desired);
    const auto result = app::commit_settings(emuenv, desired, m_app_path);
    m_config = desired.current_config;
    if (storage_path_changed)
        populate_modules_list();

    if (desired.log_level != previous_log_level)
        logging::set_level(static_cast<spdlog::level::level_enum>(desired.log_level));

    bool log_settings_changed = false;
    if (update_gui_settings && previous_buffer_size != m_ui->log_buffer_size->value()) {
        m_gui_settings->set_value(gui::l_bufferSize, m_ui->log_buffer_size->value());
        log_settings_changed = true;
    }
    const QString selected_log_font_family = m_ui->log_font_family->currentFont().family();
    if (update_gui_settings && previous_log_font_family != selected_log_font_family) {
        m_gui_settings->set_value(gui::l_fontFamily, selected_log_font_family);
        log_settings_changed = true;
    }
    if (update_gui_settings && previous_confirm_exit_app != m_ui->confirm_exit_app->isChecked())
        m_gui_settings->set_value(gui::mw_confirmExitApp, m_ui->confirm_exit_app->isChecked());
    if (update_gui_settings && previous_warn_admin_privileges != m_ui->warn_admin_privileges->isChecked())
        m_gui_settings->set_value(gui::mw_warnAdminPrivileges, m_ui->warn_admin_privileges->isChecked());
    if (update_gui_settings && previous_windows_rounded_corners != m_ui->windows_rounded_corners->isChecked())
        m_gui_settings->set_value(gui::gw_roundedCorners, m_ui->windows_rounded_corners->isChecked());
    bool theme_music_settings_changed = false;
    if (update_gui_settings && previous_theme_music_enabled != m_ui->theme_music_enable->isChecked()) {
        m_gui_settings->set_value(gui::vt_bgmEnabled, m_ui->theme_music_enable->isChecked(), false);
        theme_music_settings_changed = true;
    }
    if (update_gui_settings && previous_theme_music_volume != m_ui->theme_music_volume->value()) {
        m_gui_settings->set_value(gui::vt_bgmVolume, m_ui->theme_music_volume->value(), false);
        theme_music_settings_changed = true;
    }
    if (log_settings_changed)
        Q_EMIT gui_log_settings_request();
    if (theme_music_settings_changed)
        Q_EMIT gui_theme_music_settings_request();

    emuenv.kernel.debugger.log_imports = m_ui->log_imports->isChecked();
    emuenv.kernel.debugger.log_exports = m_ui->log_exports->isChecked();
    emuenv.kernel.debugger.dump_elfs = m_ui->dump_elfs->isChecked();
    emuenv.compat.log_compat_warn = emuenv.cfg.log_compat_warn;

    if (m_app_path.empty() && previous_user_lang != emuenv.cfg.user_lang)
        Q_EMIT ui_language_request(QString::fromStdString(emuenv.cfg.user_lang));

    return prompt_restart_if_needed(result, close_after);
}

void SettingsDialog::populate_modules_list() {
    m_ui->modules_list->clear();
    const auto modules = config::get_modules_list(emuenv.vita_fs_path, m_config.lle_modules);
    for (const auto &[name, enabled] : modules) {
        auto *item = new QListWidgetItem(QString::fromStdString(name), m_ui->modules_list);
        item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsSelectable);
        item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
    update_modules_list_enabled();
}

void SettingsDialog::update_modules_list_enabled() {
    const bool automatic = m_ui->rb_modules_automatic->isChecked();
    m_ui->modules_list->setEnabled(!automatic);
    m_ui->modules_search_bar->setEnabled(!automatic);
    m_ui->clear_modules_list->setEnabled(!automatic);
    m_ui->refresh_modules_list->setEnabled(!automatic);
}

void SettingsDialog::populate_cameras_list() {
    m_ui->front_camera_box->clear();
    m_ui->back_camera_box->clear();

    m_ui->front_camera_box->addItem(tr("Solid Color"));
    m_ui->front_camera_box->addItem(tr("Static Image"));
    m_ui->back_camera_box->addItem(tr("Solid Color"));
    m_ui->back_camera_box->addItem(tr("Static Image"));

    for (const auto &camera_name : app::get_available_camera_names()) {
        const auto camera_text = QString::fromStdString(camera_name);
        m_ui->front_camera_box->addItem(camera_text);
        m_ui->back_camera_box->addItem(camera_text);
    }

    if (emuenv.cfg.front_camera_type == 0) {
        m_ui->front_camera_box->setCurrentIndex(0);
    } else if (emuenv.cfg.front_camera_type == 1) {
        m_ui->front_camera_box->setCurrentIndex(1);
    } else {
        const int idx = m_ui->front_camera_box->findText(
            QString::fromStdString(emuenv.cfg.front_camera_id));
        m_ui->front_camera_box->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    if (emuenv.cfg.back_camera_type == 0) {
        m_ui->back_camera_box->setCurrentIndex(0);
    } else if (emuenv.cfg.back_camera_type == 1) {
        m_ui->back_camera_box->setCurrentIndex(1);
    } else {
        const int idx = m_ui->back_camera_box->findText(
            QString::fromStdString(emuenv.cfg.back_camera_id));
        m_ui->back_camera_box->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    m_ui->front_camera_image_path->setText(tr("No image selected"));
    m_ui->back_camera_image_path->setText(tr("No image selected"));

    if (!emuenv.cfg.front_camera_image.empty())
        m_ui->front_camera_image_path->setText(QString::fromUtf8(emuenv.cfg.front_camera_image));
    if (!emuenv.cfg.back_camera_image.empty())
        m_ui->back_camera_image_path->setText(QString::fromUtf8(emuenv.cfg.back_camera_image));
}

void SettingsDialog::populate_adhoc_list() {
    m_ui->adhoc_address_box->clear();
    m_ui->subnet_mask_box->clear();

    const auto addrs = app::get_available_adhoc_address_options();
    for (const auto &addr : addrs) {
        m_ui->adhoc_address_box->addItem(QString::fromStdString(addr.label));
        m_ui->subnet_mask_box->addItem(QString::fromStdString(addr.subnet_mask));
    }

    if (emuenv.cfg.adhoc_addr >= static_cast<int>(addrs.size())) {
        emuenv.cfg.adhoc_addr = 0;
        LOG_WARN("Invalid adhoc address index, resetting to 0");
    }

    if (!addrs.empty()) {
        m_ui->adhoc_address_box->setCurrentIndex(emuenv.cfg.adhoc_addr);
        m_ui->subnet_mask_box->setCurrentIndex(emuenv.cfg.adhoc_addr);
    }
}

void SettingsDialog::update_camera_widgets() {
    const int front_idx = m_ui->front_camera_box->currentIndex();
    m_ui->label_front_camera_color->setVisible(front_idx == 0);
    m_ui->front_camera_color_preview->setVisible(front_idx == 0);
    m_ui->front_camera_color_btn->setVisible(front_idx == 0);
    m_ui->label_front_camera_image->setVisible(front_idx == 1);
    m_ui->front_camera_image_path->setVisible(front_idx == 1);
    m_ui->front_camera_image_btn->setVisible(front_idx == 1);

    const int back_idx = m_ui->back_camera_box->currentIndex();
    m_ui->label_back_camera_color->setVisible(back_idx == 0);
    m_ui->back_camera_color_preview->setVisible(back_idx == 0);
    m_ui->back_camera_color_btn->setVisible(back_idx == 0);
    m_ui->label_back_camera_image->setVisible(back_idx == 1);
    m_ui->back_camera_image_path->setVisible(back_idx == 1);
    m_ui->back_camera_image_btn->setVisible(back_idx == 1);

    update_camera_color_preview();
}

void SettingsDialog::update_camera_color_preview() {
    const QColor front_color = QColor::fromRgba(emuenv.cfg.front_camera_color);
    m_ui->front_camera_color_preview->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #888;").arg(front_color.name()));

    const QColor back_color = QColor::fromRgba(emuenv.cfg.back_camera_color);
    m_ui->back_camera_color_preview->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #888;").arg(back_color.name()));
}

void SettingsDialog::update_file_loading_delay_label() {
    m_ui->file_loading_delay_label->setText(
        tr("File Loading Delay: %1 ms").arg(m_ui->file_loading_delay->value()));
}

bool SettingsDialog::apply_pending_storage_path() {
    if (m_pending_vita_fs_path == emuenv.vita_fs_path)
        return true;

    if (m_storage_path_locked) {
        m_pending_vita_fs_path = emuenv.vita_fs_path;
        update_current_emu_path_label();
        return true;
    }

    if (!app::switch_emulator_path(emuenv, m_pending_vita_fs_path)) {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to switch the emulator storage folder."));
        return false;
    }

    m_storage_path_switched = true;
    m_pending_vita_fs_path = emuenv.vita_fs_path;
    update_current_emu_path_label();
    Q_EMIT storage_path_changed();
    return true;
}

void SettingsDialog::update_http_retry_labels() {
    m_ui->label_timeout_attempts->setText(
        tr("Timeout Attempts: %1 attempts").arg(m_ui->http_timeout_attempts->value()));
    m_ui->label_timeout_sleep->setText(
        tr("Timeout Sleep: %1 ms").arg(m_ui->http_timeout_sleep->value()));
    m_ui->label_read_end_attempts->setText(
        tr("Read End Attempts: %1 attempts").arg(m_ui->http_read_end_attempts->value()));
    m_ui->label_read_end_sleep->setText(
        tr("Read End Sleep: %1 ms").arg(m_ui->http_read_end_sleep->value()));
}

void SettingsDialog::setup_connections() {
    for (auto *bb : m_button_boxes) {
        connect(bb->button(QDialogButtonBox::Save), &QPushButton::clicked,
            this, &SettingsDialog::on_save_clicked);
        connect(bb->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SettingsDialog::on_apply_clicked);
        connect(bb->button(QDialogButtonBox::Close), &QPushButton::clicked,
            this, &SettingsDialog::on_close_clicked);
    }

    connect(m_ui->settingsCategory, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;

        m_ui->tab_widget_settings->setCurrentIndex(row);
        set_description(m_ui->tab_widget_settings->widget(row), {}, m_tooltips->default_description);
    });

    connect(m_ui->backend_renderer_box, &QComboBox::currentIndexChanged,
        this, &SettingsDialog::update_gpu_visibility);

    connect(m_ui->btn_apply_stylesheet, &QPushButton::clicked, this, [this]() {
        apply_stylesheet();
    });

    connect(m_ui->rb_modules_automatic, &QRadioButton::toggled,
        this, &SettingsDialog::update_modules_list_enabled);
    connect(m_ui->rb_modules_auto_manual, &QRadioButton::toggled,
        this, &SettingsDialog::update_modules_list_enabled);
    connect(m_ui->rb_modules_manual, &QRadioButton::toggled,
        this, &SettingsDialog::update_modules_list_enabled);

    connect(m_ui->resolution_upscale, &QSlider::valueChanged,
        this, &SettingsDialog::update_resolution_label);
    connect(m_ui->resolution_upscale_reset, &QPushButton::clicked, this, [this] {
        m_ui->resolution_upscale->setValue(4); // 1.0x
    });

    connect(m_ui->anisotropic_filter, &QSlider::valueChanged,
        this, &SettingsDialog::update_aniso_label);
    connect(m_ui->anisotropic_filter_reset, &QPushButton::clicked, this, [this] {
        m_ui->anisotropic_filter->setValue(0); // 1x
    });

    connect(m_ui->audio_volume, &QSlider::valueChanged, this, [this](int val) {
        m_ui->audio_volume_label->setText(tr("Current volume: %1%").arg(val));
    });
    connect(m_ui->theme_music_volume, &QSlider::valueChanged, this, [this](int val) {
        m_ui->theme_music_volume_label->setText(tr("Theme music volume: %1%").arg(val));
    });

    connect(m_ui->perf_overlay_enabled, &QCheckBox::toggled, this, [this](bool on) {
        m_ui->label_perf_detail->setEnabled(on);
        m_ui->perf_overlay_detail_box->setEnabled(on);
        m_ui->label_perf_position->setEnabled(on);
        m_ui->perf_overlay_position_box->setEnabled(on);
    });

    connect(m_ui->file_loading_delay, &QSlider::valueChanged,
        this, &SettingsDialog::update_file_loading_delay_label);
    connect(m_ui->http_timeout_attempts, &QSlider::valueChanged,
        this, &SettingsDialog::update_http_retry_labels);
    connect(m_ui->http_timeout_sleep, &QSlider::valueChanged,
        this, &SettingsDialog::update_http_retry_labels);
    connect(m_ui->http_read_end_attempts, &QSlider::valueChanged,
        this, &SettingsDialog::update_http_retry_labels);
    connect(m_ui->http_read_end_sleep, &QSlider::valueChanged,
        this, &SettingsDialog::update_http_retry_labels);

    connect(m_ui->modules_search_bar, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < m_ui->modules_list->count(); ++i) {
            auto *item = m_ui->modules_list->item(i);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    });

    connect(m_ui->clear_modules_list, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_ui->modules_list->count(); ++i)
            m_ui->modules_list->item(i)->setCheckState(Qt::Unchecked);
    });
    connect(m_ui->refresh_modules_list, &QPushButton::clicked,
        this, &SettingsDialog::populate_modules_list);

    connect(m_ui->front_camera_box, &QComboBox::currentIndexChanged,
        this, &SettingsDialog::update_camera_widgets);
    connect(m_ui->back_camera_box, &QComboBox::currentIndexChanged,
        this, &SettingsDialog::update_camera_widgets);

    connect(m_ui->front_camera_color_btn, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(
            QColor::fromRgba(emuenv.cfg.front_camera_color), this, tr("Front Camera Color"));
        if (color.isValid()) {
            emuenv.cfg.front_camera_color = color.rgba();
            update_camera_color_preview();
        }
    });
    connect(m_ui->back_camera_color_btn, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(
            QColor::fromRgba(emuenv.cfg.back_camera_color), this, tr("Back Camera Color"));
        if (color.isValid()) {
            emuenv.cfg.back_camera_color = color.rgba();
            update_camera_color_preview();
        }
    });

    connect(m_ui->front_camera_image_btn, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Select Front Camera Image"),
            QString::fromUtf8(emuenv.cfg.front_camera_image),
            tr("Images (*.png *.jpg *.jpeg *.bmp)"));
        if (!file.isEmpty()) {
            emuenv.cfg.front_camera_image = file.toUtf8();
            m_ui->front_camera_image_path->setText(file);
        }
    });
    connect(m_ui->back_camera_image_btn, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Select Back Camera Image"),
            QString::fromUtf8(emuenv.cfg.back_camera_image),
            tr("Images (*.png *.jpg *.jpeg *.bmp)"));
        if (!file.isEmpty()) {
            emuenv.cfg.back_camera_image = file.toUtf8();
            m_ui->back_camera_image_path->setText(file);
        }
    });

    connect(m_ui->adhoc_address_box, &QComboBox::currentIndexChanged, this, [this](int idx) {
        m_ui->subnet_mask_box->setCurrentIndex(idx);
    });

    connect(m_ui->change_emu_path, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Emulator Storage Folder"),
            gui::utils::to_qt_path(m_pending_vita_fs_path));
        if (!dir.isEmpty())
            set_pending_vita_fs_path(gui::utils::to_fs_path(dir));
    });
    connect(m_ui->reset_emu_path, &QPushButton::clicked, this, [this] {
        if (emuenv.default_path != m_pending_vita_fs_path)
            set_pending_vita_fs_path(emuenv.default_path);
    });

    connect(m_ui->clear_custom_config, &QPushButton::clicked, this, [this] {
        const bool had_active_custom = !emuenv.io.app_path.empty()
            && config::has_custom_config(emuenv.config_path, emuenv.io.app_path);
        const int cleared = config::delete_all_custom_configs(emuenv.config_path);
        LOG_INFO("Cleared {} custom config settings.", cleared);

        if (!had_active_custom)
            return;

        Config desired;
        desired = emuenv.cfg;
        config::set_current_config(desired, emuenv.config_path, {});
        const auto result = app::commit_settings(emuenv, desired, {});
        prompt_restart_if_needed(result, false);
    });

    connect(m_ui->clean_shaders_cache, &QPushButton::clicked, this, [this] {
        const auto shaders_cache_path = emuenv.cache_path / "shaders";
        fs::remove_all(shaders_cache_path);
        fs::remove_all(emuenv.cache_path / "shaderlog");
        fs::remove_all(emuenv.log_path / "shaderlog");
        LOG_INFO("Shaders cache cleaned.");
    });

    connect(m_ui->watch_code, &QPushButton::clicked, this, [this] {
        emuenv.kernel.debugger.watch_code = !emuenv.kernel.debugger.watch_code;
        emuenv.kernel.debugger.update_watches();
        m_ui->watch_code->setText(emuenv.kernel.debugger.watch_code ? tr("Unwatch Code") : tr("Watch Code"));
    });
    connect(m_ui->watch_memory, &QPushButton::clicked, this, [this] {
        emuenv.kernel.debugger.watch_memory = !emuenv.kernel.debugger.watch_memory;
        emuenv.kernel.debugger.update_watches();
        m_ui->watch_memory->setText(emuenv.kernel.debugger.watch_memory ? tr("Unwatch Memory") : tr("Watch Memory"));
    });
    connect(m_ui->watch_import_calls, &QPushButton::clicked, this, [this] {
        emuenv.kernel.debugger.watch_import_calls = !emuenv.kernel.debugger.watch_import_calls;
        emuenv.kernel.debugger.update_watches();
        m_ui->watch_import_calls->setText(emuenv.kernel.debugger.watch_import_calls ? tr("Unwatch Import Calls") : tr("Watch Import Calls"));
    });

    m_ui->watch_code->setText(emuenv.kernel.debugger.watch_code ? tr("Unwatch Code") : tr("Watch Code"));
    m_ui->watch_memory->setText(emuenv.kernel.debugger.watch_memory ? tr("Unwatch Memory") : tr("Watch Memory"));
    m_ui->watch_import_calls->setText(emuenv.kernel.debugger.watch_import_calls ? tr("Unwatch Import Calls") : tr("Watch Import Calls"));

#ifdef TRACY_ENABLE
    connect(m_ui->tracy_select_none, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_ui->tracy_modules_list->count(); ++i)
            m_ui->tracy_modules_list->item(i)->setCheckState(Qt::Unchecked);
    });

    connect(m_ui->tracy_select_all, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_ui->tracy_modules_list->count(); ++i)
            m_ui->tracy_modules_list->item(i)->setCheckState(Qt::Checked);
    });
#endif

    struct DescEntry {
        QWidget *widget;
        QString title;
        QString description;
    };

    const std::vector<DescEntry> desc_widgets = {
        // Core
        { m_ui->rb_modules_automatic, tr("Automatic"), m_tooltips->modules_automatic },
        { m_ui->rb_modules_auto_manual, tr("Auto & Manual"), m_tooltips->modules_auto_manual },
        { m_ui->rb_modules_manual, tr("Manual"), m_tooltips->modules_manual },
        // CPU
        { m_ui->cpu_opt, tr("Enable CPU Optimizations"), m_tooltips->cpu_opt },
        // Graphics
        { m_ui->backend_renderer_box, tr("Backend Renderer"), m_tooltips->backend_renderer },
        { m_ui->renderer_accuracy_box, tr("Rendering Accuracy"), m_tooltips->renderer_accuracy },
        { m_ui->vsync, tr("V-Sync"), m_tooltips->vsync },
        { m_ui->disable_surface_sync, tr("Disable Surface Sync"), m_tooltips->disable_surface_sync },
        { m_ui->async_pipeline_compilation, tr("Asynchronous Pipeline Compilation"), m_tooltips->async_pipeline },
        { m_ui->memory_mapping_box, tr("Memory Mapping"), m_tooltips->memory_mapping },
        { m_ui->screen_filter_box, tr("Screen Filter"), m_tooltips->screen_filter },
        { m_ui->gb_gpu_device, tr("Graphics Device"), m_tooltips->gpu_device },
        { m_ui->gpu_device_box, tr("Graphics Device"), m_tooltips->gpu_device },
        { m_ui->resolution_upscale, tr("Internal Resolution Upscaling"), m_tooltips->resolution_upscaling },
        { m_ui->anisotropic_filter, tr("Anisotropic Filtering"), m_tooltips->anisotropic_filtering },
        { m_ui->export_textures, tr("Export Textures"), m_tooltips->export_textures },
        { m_ui->import_textures, tr("Import Textures"), m_tooltips->import_textures },
        { m_ui->gb_export_format, tr("Texture Exporting Format"), m_tooltips->texture_export_format },
        { m_ui->texture_export_format, tr("Texture Exporting Format"), m_tooltips->texture_export_format },
        { m_ui->shader_cache, tr("Enable Shader Cache"), m_tooltips->shader_cache },
        { m_ui->spirv_shader, tr("Use SPIR-V Shader"), m_tooltips->spirv_shader },
        { m_ui->fps_hack, tr("FPS Hack"), m_tooltips->fps_hack },
        // Audio
        { m_ui->audio_backend_box, tr("Audio Backend"), m_tooltips->audio_backend },
        { m_ui->audio_volume, tr("Volume"), m_tooltips->audio_volume },
        { m_ui->theme_music_enable, tr("Enable Vita Theme Background Music"), m_tooltips->theme_music_enable },
        { m_ui->theme_music_volume, tr("Theme Music Volume"), m_tooltips->theme_music_volume },
        { m_ui->ngs_enable, tr("Enable NGS Support"), m_tooltips->ngs_enable },
        // Camera
        { m_ui->front_camera_box, tr("Front Camera Source"), m_tooltips->front_camera },
        { m_ui->back_camera_box, tr("Back Camera Source"), m_tooltips->back_camera },
        // System
        { m_ui->enter_button_circle, tr("Enter Button Assignment"), m_tooltips->enter_button },
        { m_ui->enter_button_cross, tr("Enter Button Assignment"), m_tooltips->enter_button },
        { m_ui->pstv_mode, tr("PlayStation TV Mode (PSTV)"), m_tooltips->pstv_mode },
        { m_ui->show_mode, tr("Show Mode"), m_tooltips->show_mode },
        { m_ui->demo_mode, tr("Demo Mode"), m_tooltips->demo_mode },
        { m_ui->sys_lang_box, tr("System Language"), m_tooltips->sys_lang },
        { m_ui->sys_date_format_box, tr("Date Format"), m_tooltips->sys_date_format },
        { m_ui->sys_time_format_box, tr("Time Format"), m_tooltips->sys_time_format },
        { m_ui->ime_langs_list, tr("IME Languages"), m_tooltips->ime_langs },
        // Emulator
        { m_ui->boot_apps_fullscreen, tr("Boot Games in Fullscreen"), m_tooltips->boot_fullscreen },
        { m_ui->show_live_area_screen, tr("Show Live Area Before Booting"), m_tooltips->show_live_area },
        { m_ui->show_compile_shaders, tr("Show Shader Compilation Hint"), m_tooltips->show_compile_shaders },
        { m_ui->check_for_updates_mode, tr("Update Check Mode"), m_tooltips->check_for_updates },
        { m_ui->log_compat_warn, tr("Log Compatibility Warnings"), m_tooltips->log_compat_warn },
        { m_ui->texture_cache, tr("Enable Texture Cache"), m_tooltips->texture_cache },
        { m_ui->archive_log, tr("Archive Log"), m_tooltips->archive_log },
        { m_ui->label_log_level, tr("Log Level"), m_tooltips->log_level },
        { m_ui->log_level_box, tr("Log Level"), m_tooltips->log_level },
        { m_ui->discord_rich_presence, tr("Enable Discord Rich Presence"), m_tooltips->discord_rpc },
        { m_ui->perf_overlay_enabled, tr("Performance Overlay"), m_tooltips->perf_overlay },
        { m_ui->label_perf_detail, tr("Overlay Detail"), m_tooltips->perf_overlay_detail },
        { m_ui->perf_overlay_detail_box, tr("Overlay Detail"), m_tooltips->perf_overlay_detail },
        { m_ui->label_perf_position, tr("Overlay Position"), m_tooltips->perf_overlay_position },
        { m_ui->perf_overlay_position_box, tr("Overlay Position"), m_tooltips->perf_overlay_position },
        { m_ui->stretch_the_display_area, tr("Stretch the Display Area"), m_tooltips->stretch_display },
        { m_ui->fullscreen_hd_res_pixel_perfect, tr("Fullscreen HD Pixel Perfect"), m_tooltips->pixel_perfect },
        { m_ui->file_loading_delay, tr("File Loading Delay"), m_tooltips->file_loading_delay },
        { m_ui->gb_storage, tr("Emulated System Storage Folder"), m_tooltips->emulator_storage_folder },
        { m_ui->current_emu_path, tr("Emulated System Storage Folder"), m_tooltips->emulator_storage_folder },
        { m_ui->change_emu_path, tr("Emulated System Storage Folder"), m_tooltips->emulator_storage_folder },
        { m_ui->reset_emu_path, tr("Emulated System Storage Folder"), m_tooltips->emulator_storage_folder },
        { m_ui->gb_custom_config, tr("Custom Config Settings"), m_tooltips->custom_config },
        { m_ui->clear_custom_config, tr("Custom Config Settings"), m_tooltips->custom_config },
        { m_ui->label_screenshot_format, tr("Screenshot Format"), m_tooltips->screenshot_format },
        { m_ui->screenshot_format, tr("Screenshot Format"), m_tooltips->screenshot_format },
        // Interface
        { m_ui->show_welcome, tr("Show welcome screen"), m_tooltips->show_welcome_screen },
        { m_ui->warn_missing_firmware, tr("Warn when firmware is missing"), m_tooltips->warn_missing_firmware },
        { m_ui->confirm_exit_app, tr("Show exit app confirmation"), m_tooltips->confirm_exit_app },
        { m_ui->warn_admin_privileges, tr("Warn when running with administrator privileges"), m_tooltips->warn_admin_privileges },
        { m_ui->windows_rounded_corners, tr("Rounded corners"), m_tooltips->windows_rounded_corners },
        { m_ui->ui_language_box, tr("Interface Language"), m_tooltips->ui_language },
        { m_ui->log_buffer_size, tr("Log Buffer Size"), m_tooltips->log_buffer_size },
        { m_ui->log_font_family, tr("Log Font"), m_tooltips->log_font },
        { m_ui->combo_stylesheets, tr("Stylesheet"), m_tooltips->gui_stylesheet },
        // Network
        { m_ui->psn_signed_in, tr("PSN Signed In"), m_tooltips->psn_signed_in },
        { m_ui->http_enable, tr("Enable HTTP Networking"), m_tooltips->http_enable },
        { m_ui->label_timeout_attempts, tr("Timeout Attempts"), m_tooltips->http_timeout_attempts },
        { m_ui->http_timeout_attempts, tr("Timeout Attempts"), m_tooltips->http_timeout_attempts },
        { m_ui->label_timeout_sleep, tr("Timeout Sleep"), m_tooltips->http_timeout_sleep },
        { m_ui->http_timeout_sleep, tr("Timeout Sleep"), m_tooltips->http_timeout_sleep },
        { m_ui->label_read_end_attempts, tr("Read End Attempts"), m_tooltips->http_read_end_attempts },
        { m_ui->http_read_end_attempts, tr("Read End Attempts"), m_tooltips->http_read_end_attempts },
        { m_ui->label_read_end_sleep, tr("Read End Sleep"), m_tooltips->http_read_end_sleep },
        { m_ui->http_read_end_sleep, tr("Read End Sleep"), m_tooltips->http_read_end_sleep },
        { m_ui->adhoc_address_box, tr("Ad-Hoc Address"), m_tooltips->adhoc_address },
        // Debug
        { m_ui->log_imports, tr("Import Logging"), m_tooltips->log_imports },
        { m_ui->log_exports, tr("Export Logging"), m_tooltips->log_exports },
        { m_ui->log_active_shaders, tr("Log Active Shaders"), m_tooltips->log_active_shaders },
        { m_ui->log_uniforms, tr("Log Shader Uniforms"), m_tooltips->log_uniforms },
        { m_ui->color_surface_debug, tr("Save color surfaces"), m_tooltips->color_surface_debug },
        { m_ui->validation_layer, tr("Vulkan Validation Layer"), m_tooltips->validation_layer },
        { m_ui->dump_elfs, tr("ELF Dumping"), m_tooltips->dump_elfs },
        { m_ui->gb_debug_tracy_section, tr("Tracy Profiler"), m_tooltips->gb_debug_tracy_section },
        { m_ui->tracy_primitive_impl, tr("Primitive Implementation"), m_tooltips->tracy_primitive_impl },
        { m_ui->tracy_modules_list, tr("Tracy modules list"), m_tooltips->tracy_modules_list },
    };

    for (const auto &[widget, title, description] : desc_widgets) {
        widget->setProperty("_desc_title", title);
        widget->setProperty("_desc", description);
        widget->installEventFilter(this);
    }
}

// TODO: Allow this to go from enter to another enter without leaving (widget to widget without setting the default title and description)
bool SettingsDialog::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Enter) {
        auto *w = qobject_cast<QWidget *>(watched);
        if (w) {
            const QString description = w->property("_desc").toString();
            if (!description.isEmpty()) {
                const QString title = w->property("_desc_title").toString();
                QWidget *tab = w;
                while (tab && m_ui->tab_widget_settings->indexOf(tab) == -1)
                    tab = tab->parentWidget();
                if (tab)
                    set_description(tab, title, description);
            }
        }
    } else if (event->type() == QEvent::Leave) {
        auto *w = qobject_cast<QWidget *>(watched);
        if (w && !w->property("_desc").toString().isEmpty()) {
            QWidget *tab = w;
            while (tab && m_ui->tab_widget_settings->indexOf(tab) == -1)
                tab = tab->parentWidget();
            if (tab)
                set_description(tab, {}, m_tooltips->default_description);
        }
    }
    return QDialog::eventFilter(watched, event);
}

void SettingsDialog::set_description(QWidget *tab, const QString &title_override, const QString &text) {
    const int index = m_ui->tab_widget_settings->indexOf(tab);
    if (index < 0)
        return;

    const QString title = [&]() {
        if (!title_override.isEmpty())
            return title_override;
        if (auto *item = m_ui->settingsCategory->item(index))
            return item->text();
        return m_ui->tab_widget_settings->tabText(index);
    }();

    if (text == m_tooltips->default_description) {
        m_ui->helpText->setHtml(gui::utils::format_help_html(title, m_tooltips->category_summary(static_cast<SettingsTab>(index))));
    } else {
        m_ui->helpText->setHtml(gui::utils::format_help_html(title, text));
    }
}

void SettingsDialog::update_resolution_label() {
    const float mult = static_cast<float>(m_ui->resolution_upscale->value()) / 4.0f;
    const int w = static_cast<int>(960 * mult);
    const int h = static_cast<int>(544 * mult);
    m_ui->resolution_upscale_val->setText(
        QStringLiteral("%1x%2 (%3x)").arg(w).arg(h).arg(static_cast<double>(mult), 0, 'g', 3));
}

void SettingsDialog::update_aniso_label() {
    const int val = 1 << m_ui->anisotropic_filter->value();
    m_ui->anisotropic_filter_val->setText(QStringLiteral("%1x").arg(val));
}

void SettingsDialog::on_save_clicked() {
    if (!m_dirty) {
        accept();
        return;
    }

    if (!commit_changes(true))
        return;
    apply_stylesheet();
    m_dirty = false;
    accept();
}

void SettingsDialog::on_apply_clicked() {
    if (!commit_changes(false))
        return;
    apply_stylesheet();
    m_dirty = false;
    for (auto *bb : m_button_boxes)
        bb->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void SettingsDialog::on_close_clicked() {
    reject();
}

void SettingsDialog::reject() {
    if (m_dirty) {
        const auto result = gui::utils::show_message_box(
            this,
            QMessageBox::Warning,
            tr("Unsaved Changes"),
            tr("You have unsaved changes. What would you like to do?"),
            {
                { QStringLiteral("save"), tr("Save"), QMessageBox::AcceptRole, true },
                { QStringLiteral("discard"), tr("Discard"), QMessageBox::DestructiveRole, false },
                { QStringLiteral("cancel"), tr("Cancel"), QMessageBox::RejectRole, false },
            });

        if (result.clicked_id == QStringLiteral("save")) {
            if (!commit_changes(true))
                return;
            m_dirty = false;
            QDialog::accept();
            return;
        }

        if (result.clicked_id == QStringLiteral("discard")) {
            QDialog::reject();
            return;
        }

        return;
    }
    QDialog::reject();
}

void SettingsDialog::mark_dirty() {
    if (!m_dirty) {
        m_dirty = true;
        for (auto *bb : m_button_boxes)
            bb->button(QDialogButtonBox::Apply)->setEnabled(true);
    }
}

void SettingsDialog::set_pending_vita_fs_path(const fs::path &vita_fs_path) {
    const fs::path normalized_vita_fs_path = vita_fs_path / "";
    if (normalized_vita_fs_path.empty() || normalized_vita_fs_path == m_pending_vita_fs_path)
        return;

    m_pending_vita_fs_path = normalized_vita_fs_path;
    update_current_emu_path_label();
    mark_dirty();
}

void SettingsDialog::update_current_emu_path_label() {
    m_ui->current_emu_path->setText(
        tr("Current emulator path: %1").arg(gui::utils::to_qt_path(m_pending_vita_fs_path)));
}

void SettingsDialog::setup_dirty_tracking() {
    for (auto *cb : findChildren<QCheckBox *>())
        connect(cb, &QCheckBox::toggled, this, [this] { mark_dirty(); });
    for (auto *rb : findChildren<QRadioButton *>())
        connect(rb, &QRadioButton::toggled, this, [this] { mark_dirty(); });
    for (auto *combo : findChildren<QComboBox *>())
        connect(combo, &QComboBox::currentIndexChanged, this, [this] { mark_dirty(); });
    for (auto *slider : findChildren<QSlider *>())
        connect(slider, &QSlider::valueChanged, this, [this] { mark_dirty(); });
    for (auto *list : findChildren<QListWidget *>())
        connect(list, &QListWidget::itemChanged, this, [this] { mark_dirty(); });
}

void SettingsDialog::update_gpu_visibility() {
    const bool is_vulkan = m_ui->backend_renderer_box->currentText() == QStringLiteral("Vulkan");

    const int gpu_idx = m_ui->gpu_device_box->currentIndex();
    const int mask = app::get_supported_memory_mapping_mask(emuenv, gpu_idx);
    const bool has_mapping = is_vulkan && (mask > 1);

    // Vulkan-only widgets
    m_ui->gb_gpu_device->setVisible(is_vulkan);
    m_ui->gb_renderer_accuracy->setVisible(is_vulkan);
    m_ui->gb_vulkan_options->setVisible(is_vulkan);
    m_ui->spirv_shader->setVisible(is_vulkan);

    m_ui->label_memory_mapping->setVisible(has_mapping);
    m_ui->memory_mapping_box->setVisible(has_mapping);

    // OpenGL-only widgets
    m_ui->gb_opengl_options->setVisible(!is_vulkan);

    {
        const QString previous = m_ui->screen_filter_box->currentText().isEmpty()
            ? QString::fromStdString(m_config.screen_filter)
            : m_ui->screen_filter_box->currentText();

        m_ui->screen_filter_box->clear();
        if (is_vulkan) {
            m_ui->screen_filter_box->addItems({ QStringLiteral("Nearest"), QStringLiteral("Bilinear"),
                QStringLiteral("Bicubic"), QStringLiteral("FXAA"), QStringLiteral("FSR") });
        } else {
            m_ui->screen_filter_box->addItems({ QStringLiteral("Bilinear"), QStringLiteral("FXAA") });
        }

        const int idx = m_ui->screen_filter_box->findText(previous);
        m_ui->screen_filter_box->setCurrentIndex(idx >= 0 ? idx : 0);
    }
}

void SettingsDialog::add_stylesheets(const QString &preferred_name) {
    m_ui->combo_stylesheets->blockSignals(true);
    m_ui->combo_stylesheets->clear();

    const auto themes = m_theme_manager.available_themes();
    for (const auto &theme : themes)
        m_ui->combo_stylesheets->addItem(theme.display_name, theme.name);

    if (!preferred_name.isEmpty())
        m_current_stylesheet = preferred_name;
    else if (m_gui_settings)
        m_current_stylesheet = m_gui_settings->get_value(gui::m_currentStylesheet).toString();
    else
        m_current_stylesheet.clear();

    const int index = m_ui->combo_stylesheets->findData(m_current_stylesheet);
    if (index >= 0) {
        m_ui->combo_stylesheets->setCurrentIndex(index);
    } else if (m_ui->combo_stylesheets->count() > 0) {
        m_ui->combo_stylesheets->setCurrentIndex(0);
        m_current_stylesheet = m_ui->combo_stylesheets->currentData().toString();
    }
    m_ui->combo_stylesheets->blockSignals(false);
}

void SettingsDialog::apply_stylesheet(bool reset) {
    if (reset) {
        m_current_stylesheet = gui::DarkStylesheet;
        m_ui->combo_stylesheets->setCurrentIndex(
            m_ui->combo_stylesheets->findData(gui::DarkStylesheet));
    }

    m_current_stylesheet = m_ui->combo_stylesheets->currentData().toString();

    if (m_gui_settings && m_current_stylesheet != m_gui_settings->get_value(gui::m_currentStylesheet).toString()) {
        m_gui_settings->set_value(gui::m_currentStylesheet, m_current_stylesheet);
        Q_EMIT gui_stylesheet_request();
    }
}
