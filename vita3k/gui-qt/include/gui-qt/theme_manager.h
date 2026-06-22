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

#include <gui-qt/vita_theme.h>

#include <util/fs.h>

#include <QList>
#include <QObject>
#include <QPalette>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>
#include <vector>

class GuiSettings;
class QAudioOutput;
class QMediaPlayer;

namespace gui {

struct ThemeEntry {
    QString name;
    QString display_name;
    QString qss_source;
    QString base_path;
};

struct VitaThemeSelection {
    QString theme_id;
    QString background_id;
    int readability = 55;
    bool normalize_font_colors = true;
    bool cycle_enabled = false;
    int cycle_interval_seconds = 15;
    QStringList cycle_background_ids;
};

VitaThemeSelection normalize_vita_theme_selection(
    const VitaThemeInfo &theme,
    VitaThemeSelection selection);

struct ThemeBackgroundPresentation {
    QStringList image_paths;
    int interval_ms = 0;
    bool animated = false;
};

struct AppliedThemeState {
    bool is_vita_theme = false;
    ThemeBackgroundPresentation background;
};

} // namespace gui

class ThemeManager final : public QObject {
    Q_OBJECT

public:
    explicit ThemeManager(std::shared_ptr<GuiSettings> gui_settings,
        QObject *parent = nullptr);

    QList<gui::ThemeEntry> available_themes() const;
    bool apply_theme(const QString &name = {}, bool force_reload = false);
    void set_vita_fs_path(const fs::path &vita_fs_path);
    void set_vita_themes_root(const fs::path &themes_root);
    void refresh_vita_theme_catalog();
    const std::vector<gui::VitaThemeInfo> &installed_vita_themes(bool force_reload = false) const;
    void ensure_generated_theme_ready(const fs::path &themes_root);
    std::optional<gui::VitaThemeSelection> applied_vita_theme_selection() const;
    bool apply_vita_theme_selection(
        const gui::VitaThemeSelection &selection,
        bool force_reload = true);
    std::optional<gui::AppliedThemeState> current_applied_theme_state() const;
    void clear_applied_vita_theme();
    void refresh_vita_theme_bgm();
    void set_vita_theme_bgm_blocked(bool blocked);

Q_SIGNALS:
    void theme_state_changed();
    void theme_catalog_changed();

private:
    void save_applied_selection(const gui::VitaThemeSelection &selection) const;
    void switch_to_fallback_theme(const QString &reason);
    QString requested_theme_name() const;
    const gui::VitaThemeInfo *find_installed_vita_theme(const QString &theme_id, bool force_reload = false) const;
    std::optional<gui::ThemeEntry> builtin_theme(const QString &name) const;
    std::optional<gui::ThemeEntry> generated_vita_theme() const;
    QList<gui::ThemeEntry> custom_themes() const;
    std::optional<gui::ThemeEntry> resolve_theme(const QString &name) const;
    std::optional<gui::AppliedThemeState> build_applied_theme_state(const gui::ThemeEntry &theme, const QString &stylesheet) const;
    bool apply_theme_entry(const gui::ThemeEntry &theme, bool force_reload);
    bool apply_theme_entry(const gui::ThemeEntry &theme, const QString &stylesheet, bool force_reload);
    std::optional<QString> load_stylesheet(const gui::ThemeEntry &theme) const;
    std::optional<QPalette> parse_palette(const QString &stylesheet) const;
    QString resolve_stylesheet_urls(const QString &stylesheet, const gui::ThemeEntry &theme) const;
    QString resolve_stylesheet_url(const QString &url, const gui::ThemeEntry &theme) const;
    fs::path generated_vita_theme_path(const QString &theme_id) const;
    QString generated_vita_theme_display_name(const QString &theme_id) const;
    void ensure_fusion_style() const;
    void stop_vita_theme_bgm();

    std::shared_ptr<GuiSettings> m_gui_settings;
    QString m_current_theme_name;
    fs::path m_vita_fs_path;
    fs::path m_vita_themes_root;
    QAudioOutput *m_vita_theme_bgm_audio_output = nullptr;
    QMediaPlayer *m_vita_theme_bgm_player = nullptr;
    QString m_current_vita_theme_bgm_source_path;
    std::optional<gui::AppliedThemeState> m_applied_theme_state;
    mutable std::vector<gui::VitaThemeInfo> m_installed_vita_theme_cache;
    mutable bool m_vita_theme_cache_loaded = false;
    bool m_vita_theme_bgm_blocked = false;
};
