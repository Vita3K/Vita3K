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

#include <QDialog>

#include <memory>

struct EmuEnvState;

namespace Ui {
class welcome_dialog;
}

class WelcomeDialog final : public QDialog {
    Q_OBJECT

public:
    explicit WelcomeDialog(EmuEnvState &emuenv, bool is_manual_show, QWidget *parent = nullptr);
    ~WelcomeDialog();

private:
    EmuEnvState &emuenv;
    std::unique_ptr<Ui::welcome_dialog> m_ui;
};