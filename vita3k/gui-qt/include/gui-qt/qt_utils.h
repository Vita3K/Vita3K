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
#include <QMessageBox>
#include <QPalette>
#include <QPixmap>
#include <QString>

#include <util/fs.h>

#include <map>
#include <string>
#include <vector>

class QWidget;

namespace gui::utils {

struct MessageDialogButton {
    QString id;
    QString text;
    QMessageBox::ButtonRole role = QMessageBox::AcceptRole;
    bool is_default = false;
};

struct MessageDialogResult {
    QString clicked_id;
    bool checkbox_checked = false;
};

void open_dir(const fs::path &path);

fs::path to_fs_path(const QString &path);
QString to_qt_path(const fs::path &path);

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

MessageDialogResult show_message_box(
    QWidget *parent,
    QMessageBox::Icon icon,
    const QString &title,
    const QString &text,
    const std::vector<MessageDialogButton> &buttons,
    const QString &informative_text = {},
    const QString &checkbox_text = {},
    bool checkbox_checked = false,
    Qt::TextFormat text_format = Qt::AutoText,
    Qt::TextInteractionFlags text_interaction = Qt::NoTextInteraction,
    const QString &detailed_text = {});

} // namespace gui::utils
