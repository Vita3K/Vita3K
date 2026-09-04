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

#include <cheat/cheat.h>

#include <QDialog>

#include <string>

struct EmuEnvState;

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

// Cheats of the running game are toggled live, those of any other game only in memory until saved.
class CheatsDialog : public QDialog {
    Q_OBJECT

public:
    explicit CheatsDialog(EmuEnvState &emuenv, QWidget *parent = nullptr);

    void set_app(const std::string &title_id, const QString &app_title);
    void reload();

private Q_SLOTS:
    void on_item_changed(QTableWidgetItem *item);
    void on_current_row_changed(int row);
    void on_reload();
    void on_master_toggled(bool enabled);
    void on_filter_changed(const QString &text);
    void on_save();
    void on_open_file();
    void on_open_folder();

private:
    bool is_live() const;
    void set_all_enabled(bool enabled);
    void set_cheat_enabled(size_t index, bool enabled);
    void populate();
    void apply_filter();
    void update_status();
    void update_details();

    EmuEnvState &emuenv;

    std::string m_title_id;
    QString m_app_title;
    cheat::CheatFile m_file;
    // Set while the table is being filled, so that filling it does not toggle any cheat.
    bool m_populating = false;

    QCheckBox *m_master = nullptr;
    QLabel *m_header = nullptr;
    QLabel *m_status = nullptr;
    QLineEdit *m_filter = nullptr;
    QTableWidget *m_table = nullptr;
    QPlainTextEdit *m_details = nullptr;
    QPushButton *m_enable_all_button = nullptr;
    QPushButton *m_disable_all_button = nullptr;
    QPushButton *m_reload_button = nullptr;
    QPushButton *m_save_button = nullptr;
    QPushButton *m_open_file_button = nullptr;
};
