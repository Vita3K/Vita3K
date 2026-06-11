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

#include <gui-qt/apps_list.h>
#include <gui-qt/apps_list_table.h>
#include <gui-qt/gui_settings.h>
#include <gui-qt/theme_surface.h>

#include <app/functions.h>
#include <emuenv/state.h>
#include <util/log.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

AppsList::AppsList(EmuEnvState &emuenv, QWidget *parent)
    : custom_dock_widget(tr("App Library"), parent)
    , m_emuenv(emuenv) {
    setObjectName(QStringLiteral("apps_list"));
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    auto *central = new ThemeSurface(this);
    central->setObjectName(QStringLiteral("apps_list_frame"));
    auto *vbox = new QVBoxLayout(central);
    vbox->setContentsMargins(0, 4, 0, 0);
    vbox->setSpacing(4);

    m_table = new AppsListTable(central);
    m_table->viewport()->setObjectName(QStringLiteral("apps_list_viewport"));
    vbox->addWidget(m_table);

    setWidget(central);

    connect(m_table, &AppsListTable::app_boot_requested,
        this, &AppsList::on_app_boot_requested);

    connect(m_table, &AppsListTable::app_selection_changed,
        this, &AppsList::app_selection_changed);

    connect(m_table, &AppsListTable::context_menu_requested,
        this, &AppsList::context_menu_requested);
}

AppsList::~AppsList() = default;

void AppsList::refresh(bool from_disk) {
    if (from_disk)
        app::scan_apps(m_emuenv);
    else if (!app::load_cached_apps(m_emuenv))
        app::scan_apps(m_emuenv);

    app::load_app_times(m_emuenv);

    m_apps = app::get_apps(m_emuenv);
    const auto user_times = app::get_user_app_times(m_emuenv);
    m_table->populate(m_apps, user_times, m_emuenv.compat, m_emuenv.vita_fs_path, m_emuenv.config_path);

    if (!m_search_text.isEmpty())
        set_search_text(m_search_text);
}

void AppsList::resize_icons(int slider_pos) {
    const int sz = 48 + (slider_pos * (160 - 48)) / 100;
    m_table->set_icon_size(QSize(sz, sz));
}

const app::AppEntry *AppsList::selected_app() const {
    return m_table->selected_app();
}

void AppsList::set_search_text(const QString &text) {
    m_search_text = text.toLower();
    const int rows = m_table->rowCount();
    for (int r = 0; r < rows; ++r) {
        bool visible = true;
        if (!m_search_text.isEmpty()) {
            const auto *title_item = m_table->item(r, static_cast<int>(AppsListColumn::Title));
            const auto *id_item = m_table->item(r, static_cast<int>(AppsListColumn::TitleId));
            const QString title = title_item ? title_item->text().toLower() : QString();
            const QString id = id_item ? id_item->text().toLower() : QString();
            visible = title.contains(m_search_text) || id.contains(m_search_text);
        }
        m_table->setRowHidden(r, !visible);
    }
}

void AppsList::on_app_boot_requested(const app::AppEntry &app) {
    Q_EMIT boot_requested(app);
}

void AppsList::closeEvent(QCloseEvent *event) {
    Q_EMIT frame_closed();
    QDockWidget::closeEvent(event);
}

void AppsList::save_settings(GuiSettings &settings) const {
    if (!m_table)
        return;

    settings.set_value(gui::gl_headerState, m_table->save_layout(), false);
    settings.set_value(gui::gl_visibleColumns, m_table->visible_column_keys(), false);
    settings.set_value(gui::gl_sortCol, m_table->sort_column(), false);
    settings.set_value(gui::gl_sortAsc, m_table->sort_order() == Qt::AscendingOrder, true);
    settings.set_value(gui::gl_iconCrop, static_cast<int>(m_table->icon_crop()), false);
}

void AppsList::load_settings(const GuiSettings &settings) {
    if (!m_table)
        return;

    m_table->restore_visible_columns(settings.get_value(gui::gl_visibleColumns).toStringList());

    const QByteArray header_state = settings.get_value(gui::gl_headerState).toByteArray();
    if (!header_state.isEmpty())
        m_table->restore_layout(header_state);

    const int sort_col = std::clamp(settings.get_value(gui::gl_sortCol).toInt(), 0, APPS_LIST_COLUMN_COUNT - 1);
    const bool sort_asc = settings.get_value(gui::gl_sortAsc).toBool();
    m_table->sort(sort_col, sort_asc ? Qt::AscendingOrder : Qt::DescendingOrder);

    const int icon_crop = settings.get_value(gui::gl_iconCrop).toInt();
    m_table->set_icon_crop(icon_crop == static_cast<int>(AppsListIconCrop::Circle)
            ? AppsListIconCrop::Circle
            : AppsListIconCrop::Square);
}
