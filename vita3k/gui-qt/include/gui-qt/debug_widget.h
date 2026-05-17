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

#include <gui-qt/custom_dock_widget.h>

#include <QTimer>

class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QComboBox;
class QPlainTextEdit;
class QSpinBox;

struct EmuEnvState;

class DebugWidget : public custom_dock_widget {
    Q_OBJECT

public:
    explicit DebugWidget(EmuEnvState &emuenv, QWidget *parent = nullptr);

    void show_tab(int index);

    enum Tab {
        Threads = 0,
        Mutexes,
        LwMutexes,
        Condvars,
        LwCondvars,
        Semaphores,
        EventFlags,
        Allocations,
        Disassembly,
        TabCount
    };

private slots:
    void refresh_current_tab();

private:
    void setup_ui();

    QTreeWidget *create_tree(const QStringList &headers);

    void refresh_threads();
    void refresh_mutexes();
    void refresh_lw_mutexes();
    void refresh_condvars();
    void refresh_lw_condvars();
    void refresh_semaphores();
    void refresh_event_flags();
    void refresh_allocations();

    void on_disassembly_evaluate();
    void on_thread_double_clicked(QTreeWidgetItem *item, int column);

    EmuEnvState &emuenv;
    QTabWidget *m_tabs = nullptr;
    QTimer *m_refresh_timer = nullptr;

    QTreeWidget *m_threads_tree = nullptr;
    QTreeWidget *m_mutexes_tree = nullptr;
    QTreeWidget *m_lw_mutexes_tree = nullptr;
    QTreeWidget *m_condvars_tree = nullptr;
    QTreeWidget *m_lw_condvars_tree = nullptr;
    QTreeWidget *m_semaphores_tree = nullptr;
    QTreeWidget *m_event_flags_tree = nullptr;
    QTreeWidget *m_allocations_tree = nullptr;
    QWidget *m_disasm_tab = nullptr;
    QPlainTextEdit *m_disasm_output = nullptr;
    QLineEdit *m_disasm_address = nullptr;
    QSpinBox *m_disasm_count = nullptr;
    QComboBox *m_disasm_arch = nullptr;
};
