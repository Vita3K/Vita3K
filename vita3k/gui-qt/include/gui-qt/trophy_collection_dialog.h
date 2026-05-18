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

#include <np/trophy/collection.h>

#include <emuenv/state.h>

#include <QDialog>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

#include <array>
#include <memory>
#include <string>
#include <vector>

class QLabel;
class QPushButton;
class QCloseEvent;
class QStackedWidget;
class QTableView;
class QSlider;
class QWidget;
class QResizeEvent;

using TrophyEntry = np::trophy::TrophyRecord;
using TrophyAppData = np::trophy::CollectionSnapshot;

class GuiSettings;
class TrophyFilterProxy : public QSortFilterProxyModel {
public:
    explicit TrophyFilterProxy(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent) {}

    bool show_locked = true;
    bool show_unlocked = true;
    bool show_hidden = false;
    bool show_bronze = true;
    bool show_silver = true;
    bool show_gold = true;
    bool show_platinum = true;

    void invalidate() { invalidateFilter(); }

protected:
    bool filterAcceptsRow(int src_row, const QModelIndex &) const override;
};

class TrophyCollectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit TrophyCollectionDialog(EmuEnvState &emuenv,
        std::shared_ptr<GuiSettings> gui_settings);
    ~TrophyCollectionDialog() override;

    void jump_to_trophy(const QString &np_com_id, int trophy_id);

    void refresh_app(const QString &np_com_id);

public Q_SLOTS:
    void on_trophy_unlocked(const QString &np_com_id);
    void reload();

private Q_SLOTS:
    void on_app_selection_changed(int app_idx);
    void apply_filter();
    void show_trophy_context_menu(const QPoint &pos);
    void show_app_context_menu(const QPoint &pos);

private:
    void load_all_apps();
    bool load_app_data(const QString &np_com_id_dir);
    std::unique_ptr<TrophyAppData> load_app_data_entry(const QString &np_com_id_dir) const;
    void populate_app_table();
    void populate_trophy_table(int app_index);

    void load_icon_async(QStandardItemModel *model, int column, const QSize &icon_size, int row, const QString &path);
    void load_app_icon_async(int row, const QString &path);
    void load_trophy_icon_async(int row, const QString &path);

    void adjust_trophy_icon_column();
    void adjust_app_icon_column();
    void repaint_trophy_icons();
    void repaint_app_icons();
    void update_summary_progress();
    void update_page_controls();
    void update_app_column_layout();
    void update_trophy_column_layout();
    void enable_app_column_resizing();
    void enable_trophy_column_resizing();
    void initialize_app_column_layout();
    void initialize_trophy_column_layout();
    void update_app_row_height(int row);
    int default_app_row_height() const;

    static QString format_timestamp(quint64 unix_sec);
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void restore_persistent_state();
    void save_persistent_state() const;

    EmuEnvState &m_emuenv;
    std::shared_ptr<GuiSettings> m_gui_settings;
    std::shared_ptr<bool> m_trophy_cb_alive;

    std::vector<std::unique_ptr<TrophyAppData>> m_db;

    QLabel *m_progress_label = nullptr;
    QStackedWidget *m_stacked = nullptr;
    QPushButton *m_back_button = nullptr;
    QWidget *m_trophy_filters_widget = nullptr;
    QTableView *m_app_table = nullptr;
    QTableView *m_trophy_table = nullptr;
    QSlider *m_icon_slider = nullptr;

    QStandardItemModel *m_app_model = nullptr;
    QStandardItemModel *m_trophy_model = nullptr;
    QSortFilterProxyModel *m_trophy_proxy = nullptr;

    QSize m_app_icon_size{ 104, 104 };
    QSize m_trophy_icon_size{ 76, 76 };
    int m_app_slider_pos = 50;
    int m_trophy_slider_pos = 25;
    int m_selected_app_idx = -1;
    bool m_has_saved_app_header_state = false;
    bool m_has_saved_trophy_header_state = false;

    enum TrophyCol {
        TC_Icon = 0,
        TC_Name,
        TC_Detail,
        TC_Grade,
        TC_Status,
        TC_Id,
        TC_Time,
        TC_Count
    };

    enum AppCol {
        AC_Icon = 0,
        AC_Name,
        AC_Progress,
        AC_Trophies,
        AC_Count
    };
};
