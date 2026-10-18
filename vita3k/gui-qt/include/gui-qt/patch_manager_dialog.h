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

#include <patch/patch.h>

#include <QDialog>
#include <QPointer>

#include <map>

struct EmuEnvState;

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QThread;
class QTreeWidget;

class PatchManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PatchManagerDialog(EmuEnvState &emuenv, QWidget *parent = nullptr);

private:
    void reload_entries();
    void populate_tree();
    void apply_filter();
    void update_info();
    bool save();
    void download_patches();
    void on_download_finished(bool success, const QString &message);

    EmuEnvState &m_emuenv;
    PatchEntries m_entries;
    EnabledPatches m_enabled;
    std::map<std::string, std::string> m_owned_versions;

    QLineEdit *m_filter = nullptr;
    QCheckBox *m_owned_only = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLineEdit *m_title = nullptr;
    QLineEdit *m_serial = nullptr;
    QLineEdit *m_game_version = nullptr;
    QLineEdit *m_nid = nullptr;
    QLineEdit *m_patch_name = nullptr;
    QLineEdit *m_patch_version = nullptr;
    QLineEdit *m_author = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QPushButton *m_download = nullptr;
    QPointer<QThread> m_download_thread;
};
