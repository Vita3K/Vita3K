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

#include <QString>
#include <QVariant>

struct GuiSave {
    QString key;
    QString name;
    QVariant def;

    GuiSave() = default;

    GuiSave(const QString &k, const QString &n, const QVariant &d)
        : key(k)
        , name(n)
        , def(d) {}

    bool operator==(const GuiSave &rhs) const noexcept {
        return key == rhs.key && name == rhs.name && def == rhs.def;
    }
};
