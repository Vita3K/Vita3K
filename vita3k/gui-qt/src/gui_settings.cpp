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

#include <gui-qt/gui_settings.h>

#include <QDir>
#include <QDirIterator>

GuiSettings::GuiSettings(const QString &settings_dir, QObject *parent)
    : GuiSettingsBase(parent) {
    m_settings_dir = QDir(settings_dir);
    if (!m_settings_dir.exists())
        m_settings_dir.mkpath(QStringLiteral("."));

    m_settings = std::make_unique<QSettings>(
        m_settings_dir.filePath(gui::Settings + QStringLiteral(".ini")),
        QSettings::IniFormat, parent);
}

QStringList GuiSettings::get_stylesheet_entries() const {
    const QStringList name_filter = QStringList(QStringLiteral("*.qss"));
    QStringList res;

    {
        QDirIterator it(m_settings_dir.absolutePath(), name_filter, QDir::Files);
        while (it.hasNext()) {
            it.next();
            res.append(it.fileInfo().completeBaseName());
        }
    }

    res.removeDuplicates();
    res.sort();
    return res;
}
