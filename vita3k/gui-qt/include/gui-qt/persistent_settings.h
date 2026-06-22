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

#include <gui-qt/gui_settings_base.h>

#include <QString>

#include <map>

class PersistentSettings : public GuiSettingsBase {
    Q_OBJECT

public:
    explicit PersistentSettings(const QString &settings_dir, QObject *parent = nullptr);

    QString get_current_user(const QString &fallback) const;

public Q_SLOTS:
    void set_current_user(const QString &user_id, bool do_sync = true);

    void set_playtime(const QString &serial, quint64 playtime, bool do_sync = true);
    void add_playtime(const QString &serial, quint64 elapsed, bool do_sync = true);
    quint64 get_playtime(const QString &serial);

    void set_last_played(const QString &serial, const QString &date, bool do_sync = true);
    QString get_last_played(const QString &serial);

private:
    std::map<QString, quint64> m_playtime;
    std::map<QString, QString> m_last_played;
};
