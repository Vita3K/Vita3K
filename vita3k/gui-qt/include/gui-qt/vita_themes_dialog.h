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

#include <gui-qt/theme_manager.h>
#include <gui-qt/vita_theme.h>

#include <QDialog>

#include <memory>
#include <optional>
#include <vector>

class GuiSettings;
class ThemeManager;
class QListWidget;
class QLabel;
class QPushButton;
class QWidget;
class QHBoxLayout;
class QVBoxLayout;
class QSlider;
class QCheckBox;
class QSpinBox;
class QCloseEvent;
class QResizeEvent;
class QEvent;
class QButtonGroup;
class QRadioButton;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;

class VitaThemesDialog final : public QDialog {
    Q_OBJECT

public:
    explicit VitaThemesDialog(
        std::shared_ptr<GuiSettings> gui_settings,
        ThemeManager &theme_manager,
        QWidget *parent = nullptr);

public Q_SLOTS:
    void reload();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct BackgroundRow {
        QString id;
        QTreeWidgetItem *item = nullptr;
        QRadioButton *primary_radio = nullptr;
        QCheckBox *cycle_checkbox = nullptr;
    };

    void build_ui();
    void restore_persistent_state();
    void save_persistent_state() const;
    void repopulate_theme_list(const QString &preferred_theme_id = {});
    void apply_theme_selection(int row);
    void clear_selection_state();
    gui::VitaThemeSelection selection_from_ui() const;
    void load_selection_into_ui(
        gui::VitaThemeSelection selection,
        std::optional<QString> preview_background_id = std::nullopt,
        bool follow_primary_preview = false,
        bool reload_theme_ui = false);
    void load_theme_into_ui(const gui::VitaThemeInfo *theme);
    void sync_background_rows();
    void sync_preview();
    void sync_metadata();
    void sync_actions();
    void sync_cycle_controls();
    void set_selected_background(const QString &background_id);
    void set_selected_cycle_backgrounds(const QStringList &background_ids);
    void update_preview_pixmap();
    void update_package_thumbnail();
    void update_readability_label();
    void apply_selected_theme();
    void delete_selected_theme();
    void open_selected_theme_folder() const;
    void register_description(QWidget *widget, const QString &title, const QString &description);
    void set_description(const QString &title, const QString &text);
    void reset_description();
    QString default_description() const;
    void set_preview_background(const QString &background_id);
    QString resolve_preview_background_id(
        const gui::VitaThemeInfo &theme,
        const QString &background_id) const;

    const gui::VitaThemeInfo *current_theme() const;
    const gui::VitaThemeBackgroundOption *current_background() const;
    const gui::VitaThemeBackgroundOption *current_preview_background() const;

    std::shared_ptr<GuiSettings> m_gui_settings;
    ThemeManager &m_theme_manager;
    std::vector<gui::VitaThemeInfo> m_themes;

    QListWidget *m_theme_list = nullptr;
    QLabel *m_package_thumbnail = nullptr;
    QLabel *m_theme_title = nullptr;
    QLabel *m_theme_provider = nullptr;
    QLabel *m_preview = nullptr;
    QTreeWidget *m_background_list = nullptr;
    QCheckBox *m_cycle_checkbox = nullptr;
    QSpinBox *m_cycle_interval_spinbox = nullptr;
    QLabel *m_cycle_summary_value = nullptr;
    QLabel *m_version_value = nullptr;
    QLabel *m_theme_id_value = nullptr;
    QLabel *m_size_value = nullptr;
    QLabel *m_updated_value = nullptr;
    QLabel *m_background_value = nullptr;
    QLabel *m_cycle_value = nullptr;
    QSlider *m_readability_slider = nullptr;
    QLabel *m_readability_value = nullptr;
    QCheckBox *m_normalize_fonts_checkbox = nullptr;
    QPushButton *m_refresh_button = nullptr;
    QPushButton *m_apply_button = nullptr;
    QPushButton *m_delete_button = nullptr;
    QPushButton *m_open_folder_button = nullptr;
    QTextBrowser *m_help_text = nullptr;
    QButtonGroup *m_primary_background_group = nullptr;
    QString m_loaded_theme_id;
    std::vector<BackgroundRow> m_background_rows;

    QString m_selected_background_id;
    QString m_preview_background_id;
    QString m_loaded_preview_background_id;
    QStringList m_selected_cycle_background_ids;
    QPixmap m_preview_pixmap;
};
