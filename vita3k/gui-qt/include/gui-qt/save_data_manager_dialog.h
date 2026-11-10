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

#include <emuenv/state.h>

#include <QDialog>

#include <functional>
#include <memory>

class GuiSettings;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class QTableView;

class SaveDataManagerDialog : public QDialog {
    Q_OBJECT

public:
    SaveDataManagerDialog(EmuEnvState &emuenv, std::shared_ptr<GuiSettings> gui_settings,
        std::function<bool()> is_app_running, QWidget *parent = nullptr);
    ~SaveDataManagerDialog() override;

public Q_SLOTS:
    void reload();

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private:
    enum Column {
        Icon = 0,
        Title,
        Modified,
        SaveId,
        Notes,
        ColumnCount
    };

    fs::path savedata_root() const;
    fs::path selected_save_path() const;
    QString selected_save_id() const;

    void update_details();
    void update_icon_size(int slider_value);
    void import_from_vita();
    void export_for_vita();
    void delete_selected();
    bool run_with_progress(const QString &title, const std::function<bool(const std::function<void(float)> &)> &job);
    bool refuse_while_running();
    void select_save(const QString &save_id);

    EmuEnvState &m_emuenv;
    std::shared_ptr<GuiSettings> m_gui_settings;
    std::function<bool()> m_is_app_running;

    QStandardItemModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QLineEdit *m_search = nullptr;
    QSlider *m_icon_slider = nullptr;

    QLabel *m_detail_icon = nullptr;
    QLabel *m_detail_text = nullptr;
    QPushButton *m_export_button = nullptr;
    QPushButton *m_delete_button = nullptr;
    QPushButton *m_open_button = nullptr;

    int m_icon_size = 64;
    bool m_busy = false;
};
