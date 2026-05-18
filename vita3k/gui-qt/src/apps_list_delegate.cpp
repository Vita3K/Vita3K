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

#include <gui-qt/app_icon_item.h>
#include <gui-qt/apps_list_columns.h>
#include <gui-qt/apps_list_delegate.h>

#include <QApplication>
#include <QHeaderView>
#include <QPainter>
#include <QTableWidget>

AppsListDelegate::AppsListDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void AppsListDelegate::paint(QPainter *painter,
    const QStyleOptionViewItem &option,
    const QModelIndex &index) const {
    QStyleOptionViewItem opt = option;
    opt.state &= ~QStyle::State_HasFocus;

    if (index.column() == static_cast<int>(AppsListColumn::Icon)) {
        initStyleOption(&opt, index);

        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        const QVariant dec = index.data(Qt::DecorationRole);
        QRect icon_rect;
        if (dec.canConvert<QPixmap>()) {
            const QPixmap pm = dec.value<QPixmap>();
            if (!pm.isNull()) {
                painter->save();
                const QSizeF sz = pm.deviceIndependentSize();
                const int x = opt.rect.x() + qRound((opt.rect.width() - sz.width()) / 2.0);
                const int y = opt.rect.y() + qRound((opt.rect.height() - sz.height()) / 2.0);
                painter->drawPixmap(x, y, pm);
                painter->restore();
                icon_rect = QRect(x, y, qRound(sz.width()), qRound(sz.height()));
            }
        }

        const auto *table = qobject_cast<const QTableWidget *>(parent());
        if (!table)
            return;

        QRegion visible = table->visibleRegion();
        visible.translate(-table->verticalHeader()->width(),
            -table->horizontalHeader()->height());

        const QTableWidgetItem *cell = table->item(index.row(), index.column());
        if (!cell)
            return;

        if (!visible.boundingRect().intersects(table->visualItemRect(cell)))
            return;

        auto *icon_item = dynamic_cast<const AppIconItem *>(cell);
        if (!icon_item)
            return;

        icon_item->request_icon_load();

        if (icon_item->has_custom_config() && !icon_rect.isEmpty()) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setRenderHint(QPainter::TextAntialiasing);

            const QString badge_text = QStringLiteral("CC");
            QFont badge_font;
            badge_font.setPixelSize(std::max(icon_rect.height() / 5, 10));
            badge_font.setBold(true);
            const QFontMetrics fm(badge_font);

            const int pad_h = 4;
            const int pad_v = 2;
            const int bw = fm.horizontalAdvance(badge_text) + pad_h * 2;
            const int bh = fm.height() + pad_v * 2;
            const int bx = icon_rect.right() - bw - 2;
            const int by = icon_rect.bottom() - bh - 2;
            const QRect badge_rect(bx, by, bw, bh);

            painter->setBrush(QColor(0, 120, 215));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(badge_rect, bh / 3.0, bh / 3.0);

            painter->setPen(Qt::white);
            painter->setFont(badge_font);
            painter->drawText(badge_rect, Qt::AlignCenter, badge_text);

            painter->restore();
        }
    } else {
        QStyledItemDelegate::paint(painter, opt, index);
    }
}
