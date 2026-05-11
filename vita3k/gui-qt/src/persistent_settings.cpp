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

#include <gui-qt/persistent_settings.h>

#include <QDir>

namespace {
const QString s_group_playtime = QStringLiteral("Playtime");
const QString s_group_last_played = QStringLiteral("LastPlayed");
const QString s_group_meta = QStringLiteral("Meta");
const QString s_key_current_user = QStringLiteral("currentUser");
} // namespace

PersistentSettings::PersistentSettings(const QString &settings_dir, QObject *parent)
    : GuiSettingsBase(parent) {
    m_settings_dir = QDir(settings_dir);
    if (!m_settings_dir.exists())
        m_settings_dir.mkpath(QStringLiteral("."));

    m_settings = std::make_unique<QSettings>(
        m_settings_dir.filePath(QStringLiteral("persistent_settings.dat")),
        QSettings::IniFormat, parent);

    m_settings->beginGroup(s_group_playtime);
    for (const QString &key : m_settings->childKeys())
        m_playtime[key] = m_settings->value(key, 0).toULongLong();
    m_settings->endGroup();

    m_settings->beginGroup(s_group_last_played);
    for (const QString &key : m_settings->childKeys())
        m_last_played[key] = m_settings->value(key).toString();
    m_settings->endGroup();
}

QString PersistentSettings::get_current_user(const QString &fallback) const {
    return m_settings
        ? m_settings->value(s_group_meta + "/" + s_key_current_user, fallback).toString()
        : fallback;
}

void PersistentSettings::set_current_user(const QString &user_id, bool do_sync) {
    set_value(s_group_meta, s_key_current_user, user_id, do_sync);
}

void PersistentSettings::set_playtime(const QString &serial, quint64 playtime, bool do_sync) {
    m_playtime[serial] = playtime;
    set_value(s_group_playtime, serial, playtime, do_sync);
}

void PersistentSettings::add_playtime(const QString &serial, quint64 elapsed, bool do_sync) {
    const quint64 total = get_playtime(serial) + elapsed;
    set_playtime(serial, total, do_sync);
}

quint64 PersistentSettings::get_playtime(const QString &serial) {
    if (auto it = m_playtime.find(serial); it != m_playtime.end())
        return it->second;
    return 0;
}

void PersistentSettings::set_last_played(const QString &serial, const QString &date, bool do_sync) {
    m_last_played[serial] = date;
    set_value(s_group_last_played, serial, date, do_sync);
}

QString PersistentSettings::get_last_played(const QString &serial) {
    if (auto it = m_last_played.find(serial); it != m_last_played.end())
        return it->second;
    return {};
}
