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

#include <QColor>
#include <QDockWidget>
#include <QEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

class ThemeSurface;

class LogWidget : public custom_dock_widget {
    Q_OBJECT

public:
    explicit LogWidget(QWidget *parent = nullptr);
    ~LogWidget() override;

    void attach();
    static void register_callback();
    void set_log_buffer_size(int size);
    int log_buffer_size() const;
    void set_log_font_family(const QString &family);
    QString log_font_family() const;
    void repaint_text_colors();

private:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void on_log_message(const QString &msg, int level);
    void drain_pending_messages();
    void append_log_entry(const QString &msg, int level, bool preserve_scroll = true);
    void show_search_bar();
    void hide_search_bar();
    void find_text(bool backwards = false);
    void reposition_search_bar();
    void update_search_highlights();
    void update_search_ui_icons();
    void update_search_ui_text();
    int effective_buffer_size(int requested_size) const;

    QPlainTextEdit *m_log;
    QPointer<ThemeSurface> m_search_container;
    QLineEdit *m_search_edit = nullptr;
    QToolButton *m_search_prev_button = nullptr;
    QToolButton *m_search_next_button = nullptr;
    QToolButton *m_search_close_button = nullptr;
    QTextCursor m_current_search_cursor;
    QTimer *m_drain_timer = nullptr;
};
