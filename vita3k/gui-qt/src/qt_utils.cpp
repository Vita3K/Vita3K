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

#include <gui-qt/qt_utils.h>

#include <QApplication>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QRgb>
#include <QStyle>
#include <QStyleHints>
#include <QUrl>
#include <QWidget>

#include <map>

namespace gui::utils {

namespace {

QColor default_label_color(QPalette::ColorRole color_role) {
    QLabel dummy;
    dummy.ensurePolished();
    return dummy.palette().color(color_role);
}

QHash<QString, QColor> s_theme_role_colors;

QString theme_role_cache_key(const QString &role, QPalette::ColorRole color_role) {
    return role + QLatin1Char(':') + QString::number(static_cast<int>(color_role));
}

} // namespace

void open_dir(const fs::path &path) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(to_qt_path(path)));
}

fs::path to_fs_path(const QString &path) {
    return fs::path(path.toStdWString());
}

QString to_qt_path(const fs::path &path) {
    return QString::fromStdWString(path.wstring());
}

QString sanitize_relative_path_reference(const QString &path) {
    QString normalized = path.trimmed();
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalized.isEmpty())
        return {};

    static const QRegularExpression windows_drive_prefix(QStringLiteral(R"(^[A-Za-z]:/)"));
    static const QRegularExpression url_scheme_prefix(
        QStringLiteral(R"(^[A-Za-z][A-Za-z0-9+.-]*:)"),
        QRegularExpression::CaseInsensitiveOption);

    if (windows_drive_prefix.match(normalized).hasMatch() || url_scheme_prefix.match(normalized).hasMatch())
        return {};

    QString relative = normalized;
    while (relative.startsWith(QStringLiteral("./")))
        relative.remove(0, 2);
    while (relative.startsWith(QLatin1Char('/')))
        relative.remove(0, 1);

    relative = QDir::cleanPath(relative);
    if (relative.isEmpty() || relative == QStringLiteral(".")
        || relative == QStringLiteral("..")
        || relative.startsWith(QStringLiteral("../"))) {
        return {};
    }

    return relative;
}

QString resolve_relative_path_in_root(const QString &relative_path, const QString &root_path) {
    if (relative_path.isEmpty() || root_path.isEmpty())
        return {};

    const QDir root_dir(QDir::cleanPath(root_path));
    const QString resolved_path = QDir::cleanPath(root_dir.absoluteFilePath(relative_path));
    const QString root_relative = QDir::cleanPath(root_dir.relativeFilePath(resolved_path));
    if (root_relative == QStringLiteral("..") || root_relative.startsWith(QStringLiteral("../")))
        return {};

    return QDir::fromNativeSeparators(resolved_path);
}

QColor foreground_color(QWidget *widget) {
    if (widget) {
        widget->ensurePolished();
        return widget->palette().color(QPalette::ColorRole::WindowText);
    }
    return default_label_color(QPalette::ColorRole::WindowText);
}

QColor theme_role_color(
    const QString &role,
    QPalette::ColorRole color_role) {
    const QString cache_key = theme_role_cache_key(role, color_role);
    if (const auto it = s_theme_role_colors.constFind(cache_key); it != s_theme_role_colors.cend())
        return *it;

    QLabel dummy;
    dummy.setProperty("themeRole", role);
    dummy.ensurePolished();
    const QColor color = dummy.palette().color(color_role);
    s_theme_role_colors.insert(cache_key, color);
    return color;
}

QString format_help_html(const QString &title, const QString &body) {
    QString escaped = body.toHtmlEscaped();
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return QStringLiteral(
        "<div style=\"margin:0;\"><strong>%1</strong></div>"
        "<hr style=\"margin:1px 0 3px 0; border:0; border-top:1px solid rgba(255,255,255,0.28);\">"
        "<div style=\"margin:0;\">%2</div>")
        .arg(title.toHtmlEscaped(), escaped);
}

void invalidate_theme_cache() {
    s_theme_role_colors.clear();
}

QColor muted_text_color() {
    return theme_role_color(QStringLiteral("mutedText"));
}

QColor toolbar_icon_tint() {
    return theme_role_color(QStringLiteral("toolbarIconTint"));
}

void refresh_theme_state(QWidget *widget) {
    if (!widget)
        return;

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

bool dark_mode_active() {
    return QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

QIcon get_colorized_icon(const QIcon &icon,
    const QColor &source_color,
    const std::map<QIcon::Mode, QColor> &new_colors) {
    if (new_colors.empty())
        return icon;

    QIcon result;

    for (const auto &[mode, target_color] : new_colors) {
        const QList<QSize> sizes = icon.availableSizes(mode);
        const QSize sz = sizes.isEmpty() ? QSize(128, 128) : sizes.last();
        QPixmap base = icon.pixmap(sz, mode);

        if (base.isNull())
            base = icon.pixmap(sz);
        if (base.isNull())
            continue;

        QImage img = base.toImage().convertToFormat(QImage::Format_ARGB32);

        const int sr = source_color.red();
        const int sg = source_color.green();
        const int sb = source_color.blue();

        for (int y = 0; y < img.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                const int alpha = qAlpha(line[x]);
                if (alpha == 0)
                    continue;

                const int r = qRed(line[x]);
                const int g = qGreen(line[x]);
                const int b = qBlue(line[x]);

                const int dist = qMax(qAbs(r - sr), qMax(qAbs(g - sg), qAbs(b - sb)));

                const float factor = qMax(0.0f, 1.0f - static_cast<float>(dist) / 255.0f);
                const int new_alpha = static_cast<int>(alpha * factor);

                if (new_alpha == 0) {
                    line[x] = 0;
                } else {
                    line[x] = qRgba(target_color.red(), target_color.green(),
                        target_color.blue(), new_alpha);
                }
            }
        }

        result.addPixmap(QPixmap::fromImage(img), mode);
    }

    return result;
}

MessageDialogResult show_message_box(
    QWidget *parent,
    QMessageBox::Icon icon,
    const QString &title,
    const QString &text,
    const std::vector<MessageDialogButton> &buttons,
    const QString &informative_text,
    const QString &checkbox_text,
    bool checkbox_checked,
    Qt::TextFormat text_format,
    Qt::TextInteractionFlags text_interaction,
    const QString &detailed_text) {
    QMessageBox box(icon, title, text, QMessageBox::NoButton, parent,
        Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    box.setTextFormat(text_format);
    box.setTextInteractionFlags(text_interaction);
    box.setWindowModality(Qt::WindowModal);

    if (!informative_text.isEmpty())
        box.setInformativeText(informative_text);
    if (!detailed_text.isEmpty())
        box.setDetailedText(detailed_text);

    if (!checkbox_text.isEmpty()) {
        auto *checkbox = new QCheckBox(checkbox_text, &box);
        checkbox->setChecked(checkbox_checked);
        box.setCheckBox(checkbox);
    }

    std::vector<QAbstractButton *> button_widgets;
    button_widgets.reserve(buttons.size());
    QPushButton *default_button = nullptr;

    for (const auto &button_spec : buttons) {
        auto *button = box.addButton(button_spec.text, button_spec.role);
        if (button_spec.is_default)
            default_button = qobject_cast<QPushButton *>(button);
        button_widgets.push_back(button);
    }

    MessageDialogResult result;

    if (!default_button && !button_widgets.empty())
        default_button = qobject_cast<QPushButton *>(button_widgets.front());
    if (default_button)
        box.setDefaultButton(default_button);

    box.exec();

    for (size_t i = 0; i < button_widgets.size(); ++i) {
        if (box.clickedButton() == button_widgets[i]) {
            result.clicked_id = buttons[i].id;
            break;
        }
    }

    if (auto *checkbox = box.checkBox())
        result.checkbox_checked = checkbox->isChecked();

    return result;
}

} // namespace gui::utils
