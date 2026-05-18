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

#include <app/state.h>

#include <QDialog>
#include <QListWidget>
#include <QPushButton>

struct EmuEnvState;

class UserManagementDialog : public QDialog {
    Q_OBJECT
public:
    explicit UserManagementDialog(EmuEnvState &emuenv, QWidget *parent = nullptr);

signals:
    void user_changed();

private slots:
    void on_create_user();
    void on_delete_user();
    void on_select_user();

private:
    void refresh_list();

    EmuEnvState &emuenv;
    QListWidget *m_list = nullptr;
    QPushButton *m_select_btn = nullptr;
    QPushButton *m_delete_btn = nullptr;
};
