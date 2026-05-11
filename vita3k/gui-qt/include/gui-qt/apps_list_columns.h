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

#include <QObject>
#include <QString>
#include <QStringList>
#include <optional>

enum class AppsListColumn : int {
    Icon = 0,
    Title,
    TitleId,
    Version,
    Category,
    CompatStatus,
    LastPlayed,
    PlayTime,
    ParentalLevel,
    SizeOnDisk,
    Count
};

enum class AppsListIconCrop : int {
    Square = 0,
    Circle
};

constexpr int APPS_LIST_COLUMN_COUNT = static_cast<int>(AppsListColumn::Count);

inline QStringList apps_list_column_labels() {
    return {
        QStringLiteral("Icon"),
        QObject::tr("Title"),
        QObject::tr("Title ID"),
        QObject::tr("Version"),
        QObject::tr("Category"),
        QObject::tr("Compatibility"),
        QObject::tr("Last Played"),
        QObject::tr("Time Played"),
        QObject::tr("Parental Level"),
        QObject::tr("Size on Disk"),
    };
}

inline QString apps_list_column_key(const AppsListColumn column) {
    switch (column) {
    case AppsListColumn::Icon: return QStringLiteral("icon");
    case AppsListColumn::Title: return QStringLiteral("title");
    case AppsListColumn::TitleId: return QStringLiteral("title_id");
    case AppsListColumn::Version: return QStringLiteral("version");
    case AppsListColumn::Category: return QStringLiteral("category");
    case AppsListColumn::CompatStatus: return QStringLiteral("compatibility");
    case AppsListColumn::LastPlayed: return QStringLiteral("last_played");
    case AppsListColumn::PlayTime: return QStringLiteral("time_played");
    case AppsListColumn::ParentalLevel: return QStringLiteral("parental_level");
    case AppsListColumn::SizeOnDisk: return QStringLiteral("size_on_disk");
    case AppsListColumn::Count: break;
    }

    return QString();
}

inline std::optional<AppsListColumn> apps_list_column_from_key(const QString &key) {
    for (int i = 0; i < APPS_LIST_COLUMN_COUNT; ++i) {
        const auto column = static_cast<AppsListColumn>(i);
        if (apps_list_column_key(column) == key)
            return column;
    }

    return std::nullopt;
}

inline QString app_category_label(const std::string &category) {
    if (category == "gd")
        return QObject::tr("Game Digital Application");
    if (category == "gp")
        return QObject::tr("Game Patch");
    return QString::fromStdString(category);
}
