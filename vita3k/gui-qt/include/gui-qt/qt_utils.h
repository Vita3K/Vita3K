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

#include <QColor>
#include <QIcon>
#include <QPalette>
#include <QPixmap>

#include <util/fs.h>

#include <map>
#include <string>

namespace gui::utils {

void open_dir(const std::string &path);

fs::path to_fs_path(const QString &path);

QColor get_foreground_color(QWidget *widget = nullptr);

QIcon get_colorized_icon(const QIcon &icon,
    const QColor &source_color,
    const std::map<QIcon::Mode, QColor> &new_colors);

bool dark_mode_active();

QColor get_label_color(
    const QString &object_name,
    const QColor &fallback_light,
    const QColor &fallback_dark,
    QPalette::ColorRole color_role = QPalette::WindowText);

QColor get_label_color(
    const QColor &fallback_light,
    const QColor &fallback_dark);

} // namespace gui::utils
