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
#include <compat/state.h>
#include <gui-qt/custom_dock_widget.h>

#include <QDockWidget>
#include <QLineEdit>

#include <string>

struct EmuEnvState;
class AppsListTable;

class AppsList : public custom_dock_widget {
    Q_OBJECT

public:
    explicit AppsList(EmuEnvState &emuenv, QWidget *parent = nullptr);
    ~AppsList() override;

    void refresh(bool from_disk = false);
    void resize_icons(int slider_pos);
    void set_search_text(const QString &text);
    const app::AppEntry *selected_app() const;

    void save_settings(class GuiSettings &settings) const;
    void load_settings(const class GuiSettings &settings);

Q_SIGNALS:
    void boot_requested(const app::AppEntry &app);
    void app_selection_changed(const app::AppEntry *app);
    void context_menu_requested(const QPoint &global_pos, const std::vector<const app::AppEntry *> &apps);
    void frame_closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
    void on_app_boot_requested(const app::AppEntry &app);

private:
    EmuEnvState &m_emuenv;
    AppsListTable *m_table{};
    QString m_search_text;

    std::vector<app::AppEntry> m_apps;
};
