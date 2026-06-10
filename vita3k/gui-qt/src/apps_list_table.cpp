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

#include "gui-qt/qt_utils.h"

#include <gui-qt/app_icon_item.h>
#include <gui-qt/apps_list_delegate.h>
#include <gui-qt/apps_list_table.h>

#include <config/settings.h>
#include <util/log.h>

#include <QActionGroup>
#include <QDateTime>
#include <QDirIterator>
#include <QHeaderView>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTableWidgetItem>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>
#include <limits>

namespace {

class SortableItem : public QTableWidgetItem {
public:
    SortableItem()
        : QTableWidgetItem() {}

    explicit SortableItem(const QString &text,
        std::optional<QVariant> sort_key = std::nullopt)
        : QTableWidgetItem(text) {
        if (sort_key)
            setData(Qt::UserRole, *sort_key);
    }

    bool operator<(const QTableWidgetItem &other) const override {
        const QVariant a = data(Qt::UserRole);
        const QVariant b = other.data(Qt::UserRole);
        if (a.isValid() && b.isValid())
            return QVariant::compare(a, b) < 0;
        return text().toLower() < other.text().toLower();
    }
};

QString format_play_time(int64_t seconds) {
    if (seconds <= 0)
        return QObject::tr("Never played");
    const int64_t h = seconds / 3600;
    const int64_t m = (seconds % 3600) / 60;
    if (h > 0)
        return QStringLiteral("%1h %2m").arg(h).arg(m, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1m").arg(m);
}

quint64 directory_size(const QString &path) {
    quint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

QString format_size(quint64 bytes) {
    constexpr double GB = 1024.0 * 1024.0 * 1024.0;
    constexpr double MB = 1024.0 * 1024.0;
    constexpr double KB = 1024.0;
    if (bytes >= GB)
        return QStringLiteral("%1 GB").arg(bytes / GB, 0, 'f', 2);
    if (bytes >= MB)
        return QStringLiteral("%1 MB").arg(bytes / MB, 0, 'f', 2);
    if (bytes >= KB)
        return QStringLiteral("%1 KB").arg(bytes / KB, 0, 'f', 2);
    return QStringLiteral("%1 B").arg(bytes);
}

int parental_level_sort_key(const std::string &level) {
    bool ok = false;
    const int parsed = QString::fromStdString(level).toInt(&ok);
    return ok ? parsed : std::numeric_limits<int>::max();
}

} // namespace

AppsListTable::AppsListTable(QWidget *parent)
    : QTableWidget(parent) {
    setObjectName(QStringLiteral("apps_list_table"));
    setShowGrid(false);
    setFocusPolicy(Qt::NoFocus);
    setItemDelegate(new AppsListDelegate(this));
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(20);
    horizontalScrollBar()->setSingleStep(20);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAlternatingRowColors(false);
    setMouseTracking(true);
    setColumnCount(APPS_LIST_COLUMN_COUNT);

    setup_header();
    verticalHeader()->setDefaultSectionSize(m_icon_size.height());

    connect(horizontalHeader(), &QHeaderView::sectionClicked,
        this, &AppsListTable::on_column_header_clicked);
    connect(horizontalHeader(), &QHeaderView::customContextMenuRequested,
        this, &AppsListTable::show_header_context_menu);

    connect(this, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto apps = selected_apps();
        if (apps.empty())
            return;
        Q_EMIT context_menu_requested(viewport()->mapToGlobal(pos), apps);
    });
}

void AppsListTable::setup_header() {
    QHeaderView *hdr = horizontalHeader();
    hdr->setHighlightSections(false);
    hdr->setSortIndicatorShown(true);
    hdr->setStretchLastSection(true);
    hdr->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hdr->setDefaultSectionSize(150);
    hdr->setSectionsMovable(true);
    hdr->setContextMenuPolicy(Qt::CustomContextMenu);

    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setVisible(false);

    const QStringList labels = apps_list_column_labels();
    for (int i = 0; i < labels.size(); ++i)
        horizontalHeaderItem(i) ? horizontalHeaderItem(i)->setText(labels[i])
                                : setHorizontalHeaderItem(i, new QTableWidgetItem(labels[i]));

    hdr->setSectionResizeMode(static_cast<int>(AppsListColumn::Icon),
        QHeaderView::Fixed);
    hdr->setSortIndicator(m_sort_column, m_sort_order);
}

void AppsListTable::set_icon_size(QSize size) {
    m_icon_size = size;
    adjust_icon_column();
    repaint_icons();
}

void AppsListTable::set_icon_crop(const AppsListIconCrop crop) {
    if (m_icon_crop == crop)
        return;

    m_icon_crop = crop;
    repaint_icons();
}

void AppsListTable::repaint_icons() {
    const int icon_col = static_cast<int>(AppsListColumn::Icon);
    for (int r = 0; r < rowCount(); ++r) {
        auto *icon_item = dynamic_cast<AppIconItem *>(item(r, icon_col));
        if (!icon_item)
            continue;

        QPixmap orig = icon_item->pixmap_copy();
        if (orig.isNull()) {
            const QVariant dec = icon_item->data(Qt::DecorationRole);
            if (dec.canConvert<QPixmap>())
                orig = dec.value<QPixmap>();
        }
        if (orig.isNull())
            continue;

        icon_item->setData(Qt::DecorationRole, render_icon_pixmap(orig));
    }
}

void AppsListTable::adjust_icon_column() {
    const int icon_col = static_cast<int>(AppsListColumn::Icon);
    const int h = m_icon_size.height();
    const int w = m_icon_size.width();
    verticalHeader()->setDefaultSectionSize(h);
    verticalHeader()->setMinimumSectionSize(h);
    verticalHeader()->setMaximumSectionSize(h);
    horizontalHeader()->resizeSection(icon_col, w);
}

void AppsListTable::resize_columns_to_contents(int spacing) {
    const int icon_col = static_cast<int>(AppsListColumn::Icon);
    horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    horizontalHeader()->resizeSection(icon_col, m_icon_size.width());
    const bool stretch_last = horizontalHeader()->stretchLastSection();

    int last_visible = -1;
    int last_visual_index = -1;
    for (int i = 0; i < columnCount(); ++i) {
        if (isColumnHidden(i))
            continue;

        const int visual = horizontalHeader()->visualIndex(i);
        if (visual > last_visual_index) {
            last_visual_index = visual;
            last_visible = i;
        }
    }

    for (int i = 1; i < columnCount(); ++i) {
        if (isColumnHidden(i))
            continue;
        if (stretch_last && i == last_visible)
            continue;
        horizontalHeader()->resizeSection(i, horizontalHeader()->sectionSize(i) + spacing);
    }
}

void AppsListTable::restore_layout(const QByteArray &state) {
    if (state.isEmpty())
        return;

    horizontalHeader()->restoreState(state);
}

QByteArray AppsListTable::save_layout() const {
    return horizontalHeader()->saveState();
}

QStringList AppsListTable::visible_column_keys() const {
    QStringList visible_columns;
    for (int i = 0; i < columnCount(); ++i) {
        if (!isColumnHidden(i))
            visible_columns.append(apps_list_column_key(static_cast<AppsListColumn>(i)));
    }
    return visible_columns;
}

void AppsListTable::restore_visible_columns(const QStringList &column_keys) {
    if (column_keys.isEmpty())
        return;

    std::vector<bool> should_show(columnCount(), false);
    for (const QString &key : column_keys) {
        const auto column = apps_list_column_from_key(key);
        if (!column)
            continue;

        should_show[static_cast<int>(*column)] = true;
    }

    if (std::none_of(should_show.begin(), should_show.end(), [](const bool visible) { return visible; }))
        should_show[static_cast<int>(AppsListColumn::Title)] = true;

    for (int i = 0; i < columnCount(); ++i)
        setColumnHidden(i, !should_show[i]);
}

void AppsListTable::sort(int column, Qt::SortOrder order) {
    m_sort_column = column;
    m_sort_order = order;

    const int old_rows = rowCount();

    std::vector<int> to_rehide;
    for (int i = 0; i < columnCount(); ++i) {
        if (isColumnHidden(i)) {
            setColumnHidden(i, false);
            to_rehide.push_back(i);
        }
    }

    sortByColumn(column, order);

    for (int col : to_rehide)
        setColumnHidden(col, true);

    if (!rowCount()) {
        // preserve column widths
        return;
    }

    adjust_icon_column();

    if (!old_rows)
        resize_columns_to_contents();

    horizontalHeader()->setSortIndicator(m_sort_column, m_sort_order);
    horizontalHeader()->setSectionResizeMode(static_cast<int>(AppsListColumn::Icon),
        QHeaderView::Fixed);
}

void AppsListTable::populate(const std::vector<app::AppEntry> &apps,
    const std::map<std::string, app::AppTime> &user_times,
    const CompatState &compat,
    const fs::path &vita_fs_path,
    const fs::path &config_path) {
    struct PendingSizeLoad {
        QPersistentModelIndex index;
        QString app_dir;
        std::string app_key;
    };

    m_sort_refresh_queued = false;
    clearContents();
    verticalHeader()->setDefaultSectionSize(m_icon_size.height());
    setRowCount(static_cast<int>(apps.size()));

    std::vector<AppIconItem *> pending_icon_loads;
    std::vector<PendingSizeLoad> pending_size_loads;
    pending_icon_loads.reserve(apps.size());
    pending_size_loads.reserve(apps.size());

    int row = 0;
    for (const app::AppEntry &app : apps) {
        auto *icon_item = new AppIconItem();
        icon_item->set_app(&app);
        icon_item->set_has_custom_config(config::has_custom_config(config_path, app.path));

        const fs::path icon_path = app.icon_path;
        const std::string resolved_icon = app.icon_path.empty()
            ? std::string()
            : (icon_path.is_absolute() ? icon_path.generic_string() : (vita_fs_path / icon_path).generic_string());

        setItem(row, static_cast<int>(AppsListColumn::Icon), icon_item);
        const QPersistentModelIndex icon_index = model()->index(row, static_cast<int>(AppsListColumn::Icon));
        pending_icon_loads.push_back(icon_item);

        icon_item->set_icon_load_func([this, icon_index, resolved_icon]() {
            QThreadPool::globalInstance()->start([this, icon_index, resolved_icon]() {
                QImage img;
                if (!resolved_icon.empty())
                    img.load(QString::fromStdString(resolved_icon));

                QMetaObject::invokeMethod(
                    this, [this, icon_index, img]() {
                        if (!icon_index.isValid())
                            return;

                        auto *icon_item = dynamic_cast<AppIconItem *>(item(icon_index.row(), icon_index.column()));
                        if (!icon_item)
                            return;

                        QPixmap px;
                        if (img.isNull()) {
                            px = QPixmap(m_icon_size);
                            px.fill(Qt::transparent);
                        } else {
                            px = QPixmap::fromImage(img);
                        }

                        icon_item->set_pixmap(px);
                        icon_item->setData(Qt::DecorationRole, render_icon_pixmap(px));
                        viewport()->update(visualItemRect(icon_item));
                    },
                    Qt::QueuedConnection);
            });
        });

        setItem(row, static_cast<int>(AppsListColumn::Title),
            new SortableItem(QString::fromStdString(app.title)));
        setItem(row, static_cast<int>(AppsListColumn::TitleId),
            new SortableItem(QString::fromStdString(app.title_id)));
        setItem(row, static_cast<int>(AppsListColumn::Version),
            new SortableItem(QString::fromStdString(app.app_ver)));
        setItem(row, static_cast<int>(AppsListColumn::Category),
            new SortableItem(app_category_label(app.category)));

        CompatibilityState compat_state = CompatibilityState::UNKNOWN;
        if (const auto it = compat.app_compat_db.find(app.title_id);
            it != compat.app_compat_db.end()) {
            compat_state = it->second.state;
        }

        auto *compat_item = new SortableItem(
            QString(),
            QVariant(static_cast<int>(compat_state)));

        const QPixmap pill = compat_pixmap(compat_state);
        if (!pill.isNull())
            compat_item->setData(Qt::DecorationRole, pill);

        compat_item->setTextAlignment(Qt::AlignCenter);
        setItem(row, static_cast<int>(AppsListColumn::CompatStatus), compat_item);

        const auto time_it = user_times.find(app.path);
        const std::time_t last_played = (time_it != user_times.end()) ? time_it->second.last_time_used : 0;
        const int64_t time_played = (time_it != user_times.end()) ? time_it->second.time_used : 0;

        QString last_played_str;
        QVariant last_played_sort;
        if (last_played != 0) {
            const QDateTime lp = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(last_played));
            const bool recent = lp >= QDateTime::currentDateTime().addDays(-7);
            last_played_str = lp.toString(recent ? QStringLiteral("ddd hh:mm")
                                                 : QStringLiteral("yyyy-MM-dd"));
            last_played_sort = QVariant(static_cast<qlonglong>(last_played));
        } else {
            last_played_str = tr("Never");
            last_played_sort = QVariant(static_cast<qlonglong>(0));
        }
        setItem(row, static_cast<int>(AppsListColumn::LastPlayed),
            new SortableItem(last_played_str, last_played_sort));

        setItem(row, static_cast<int>(AppsListColumn::PlayTime),
            new SortableItem(format_play_time(time_played),
                QVariant(static_cast<qlonglong>(time_played))));

        setItem(row, static_cast<int>(AppsListColumn::ParentalLevel),
            new SortableItem(QString::fromStdString(app.parental_level),
                QVariant(parental_level_sort_key(app.parental_level))));

        const QString app_dir = gui::utils::to_qt_path(vita_fs_path / "ux0/app" / app.path);
        const auto size_it = m_size_cache.find(app.path);
        const quint64 cached_size = (size_it != m_size_cache.end()) ? size_it->second : 0;
        auto *size_item = new SortableItem(
            (size_it != m_size_cache.end()) ? format_size(cached_size) : tr("Calculating..."),
            QVariant::fromValue<qulonglong>(cached_size));
        setItem(row, static_cast<int>(AppsListColumn::SizeOnDisk), size_item);

        if (size_it == m_size_cache.end()) {
            pending_size_loads.push_back({ model()->index(row, static_cast<int>(AppsListColumn::SizeOnDisk)),
                app_dir,
                app.path });
        }

        ++row;
    }

    for (AppIconItem *icon_item : pending_icon_loads) {
        if (icon_item)
            icon_item->request_icon_load();
    }

    for (const PendingSizeLoad &pending : pending_size_loads) {
        QThreadPool::globalInstance()->start([this, pending]() {
            const quint64 size = directory_size(pending.app_dir);
            QMetaObject::invokeMethod(
                this, [this, pending, size]() {
                    m_size_cache[pending.app_key] = size;
                    if (!pending.index.isValid())
                        return;

                    auto *size_item = dynamic_cast<SortableItem *>(item(pending.index.row(), pending.index.column()));
                    if (!size_item)
                        return;

                    size_item->setText(format_size(size));
                    size_item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(size));
                    if (m_sort_column == static_cast<int>(AppsListColumn::SizeOnDisk))
                        queue_sort_refresh();
                },
                Qt::QueuedConnection);
        });
    }

    adjust_icon_column();
    horizontalHeader()->setSectionResizeMode(static_cast<int>(AppsListColumn::Icon),
        QHeaderView::Fixed);
    sort(m_sort_column, m_sort_order);

    Q_EMIT list_refreshed();
}

int AppsListTable::visible_column_count() const {
    int count = 0;
    for (int i = 0; i < columnCount(); ++i) {
        if (!isColumnHidden(i))
            ++count;
    }
    return count;
}

bool AppsListTable::can_hide_column(int column) const {
    return !isColumnHidden(column) && (visible_column_count() > 1);
}

void AppsListTable::set_column_hidden_with_guard(int column, bool hidden) {
    if (hidden && !can_hide_column(column))
        return;

    setColumnHidden(column, hidden);
}

void AppsListTable::queue_sort_refresh() {
    if (m_sort_refresh_queued)
        return;

    m_sort_refresh_queued = true;
    QTimer::singleShot(0, this, [this]() {
        m_sort_refresh_queued = false;
        sort(m_sort_column, m_sort_order);
    });
}

void AppsListTable::show_header_context_menu(const QPoint &pos) {
    QMenu menu(this);
    const QStringList labels = apps_list_column_labels();

    for (int i = 0; i < columnCount(); ++i) {
        QAction *action = menu.addAction(labels.value(i));
        action->setCheckable(true);
        action->setChecked(!isColumnHidden(i));
        connect(action, &QAction::toggled, this, [this, action, i](const bool checked) {
            if (!checked && !can_hide_column(i)) {
                const QSignalBlocker blocker(action);
                action->setChecked(true);
                return;
            }

            set_column_hidden_with_guard(i, !checked);
        });
    }

    menu.addSeparator();
    auto *icon_crop_menu = menu.addMenu(tr("Icon Crop"));
    QAction *square = icon_crop_menu->addAction(tr("Square"));
    QAction *circle = icon_crop_menu->addAction(tr("Circle"));
    auto *crop_group = new QActionGroup(icon_crop_menu);
    crop_group->setExclusive(true);
    crop_group->addAction(square);
    crop_group->addAction(circle);
    square->setCheckable(true);
    circle->setCheckable(true);
    square->setChecked(m_icon_crop == AppsListIconCrop::Square);
    circle->setChecked(m_icon_crop == AppsListIconCrop::Circle);

    connect(square, &QAction::triggered, this, [this] { set_icon_crop(AppsListIconCrop::Square); });
    connect(circle, &QAction::triggered, this, [this] { set_icon_crop(AppsListIconCrop::Circle); });

    menu.exec(horizontalHeader()->viewport()->mapToGlobal(pos));
}

QPixmap AppsListTable::render_icon_pixmap(const QPixmap &source) const {
    const qreal dpr = devicePixelRatioF();
    const QSize target_px = m_icon_size * dpr;
    if (source.isNull()) {
        QPixmap empty(target_px);
        empty.fill(Qt::transparent);
        empty.setDevicePixelRatio(dpr);
        return empty;
    }

    QPixmap rendered;
    if (m_icon_crop == AppsListIconCrop::Circle) {
        const QImage source_image = source.toImage();
        const int side = std::min(source_image.width(), source_image.height());
        const QRect source_rect((source_image.width() - side) / 2,
            (source_image.height() - side) / 2,
            side, side);
        const QImage scaled = source_image.copy(source_rect)
                                  .scaled(target_px, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        QImage masked(target_px, QImage::Format_ARGB32_Premultiplied);
        masked.fill(Qt::transparent);

        QPainter painter(&masked);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF target_rect(0, 0, target_px.width(), target_px.height());
        QPainterPath clip;
        clip.addEllipse(target_rect.adjusted(0.5, 0.5, -0.5, -0.5));
        painter.setClipPath(clip);
        painter.drawImage(target_rect, scaled);
        painter.end();

        rendered = QPixmap::fromImage(masked);
    } else {
        rendered = source.scaled(target_px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    rendered.setDevicePixelRatio(dpr);
    return rendered;
}

QString AppsListTable::compat_text(const CompatibilityState state) const {
    switch (state) {
    case CompatibilityState::PLAYABLE: return tr("Playable");
    case CompatibilityState::INGAME_MORE: return tr("Ingame+");
    case CompatibilityState::INGAME_LESS: return tr("Ingame");
    case CompatibilityState::MENU: return tr("Menu");
    case CompatibilityState::INTRO: return tr("Intro");
    case CompatibilityState::BOOTABLE: return tr("Boots");
    case CompatibilityState::NOTHING: return tr("Nothing");
    default: return tr("Unknown");
    }
}

QColor AppsListTable::compat_color(const CompatibilityState state) const {
    switch (state) {
    case CompatibilityState::UNKNOWN: return QColor(0x8a, 0x8a, 0x8a); // #8a8a8a
    case CompatibilityState::NOTHING: return QColor(0xff, 0x00, 0x00); // #ff0000
    case CompatibilityState::BOOTABLE: return QColor(0x62, 0x1f, 0xa5); // #621fa5
    case CompatibilityState::INTRO: return QColor(0xc7, 0x15, 0x85); // #c71585
    case CompatibilityState::MENU: return QColor(0x1d, 0x76, 0xdb); // #1d76db
    case CompatibilityState::INGAME_LESS: return QColor(0xe0, 0x8a, 0x1e); // #e08a1e
    case CompatibilityState::INGAME_MORE: return QColor(0xff, 0xd7, 0x00); // #ffd700
    case CompatibilityState::PLAYABLE: return QColor(0x0e, 0x8a, 0x16); // #0e8a16
    default: return {};
    }
}

QPixmap AppsListTable::compat_pixmap(const CompatibilityState state) const {
    const QString text = compat_text(state);
    const QColor bg = compat_color(state);
    if (!bg.isValid())
        return {};

    QFont font;
    font.setPointSize(9);
    font.setBold(true);
    const QFontMetrics fm(font);

    const int pad_h = 10;
    const int pad_v = 4;
    const int w = fm.horizontalAdvance(text) + pad_h * 2;
    const int h = fm.height() + pad_v * 2;

    const qreal dpr = devicePixelRatioF();
    QPixmap pm(QSize(w, h) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRect(0, 0, w, h), h / 2.0, h / 2.0);

    const bool dark = (bg.red() * 299 + bg.green() * 587 + bg.blue() * 114) / 1000 < 128;
    p.setPen(dark ? Qt::white : Qt::black);
    p.setFont(font);
    p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, text);

    return pm;
}

app::AppEntry AppsListTable::app_from_row(int row) const {
    if (const auto *icon_item = dynamic_cast<const AppIconItem *>(
            item(row, static_cast<int>(AppsListColumn::Icon)))) {
        if (const app::AppEntry *app = icon_item->app())
            return *app;
    }
    return {};
}

const app::AppEntry *AppsListTable::selected_app() const {
    const int row = currentRow();
    if (row < 0)
        return nullptr;
    if (const auto *icon_item = dynamic_cast<const AppIconItem *>(
            item(row, static_cast<int>(AppsListColumn::Icon)))) {
        return icon_item->app();
    }
    return nullptr;
}

std::vector<const app::AppEntry *> AppsListTable::selected_apps() const {
    std::vector<const app::AppEntry *> result;
    const int icon_col = static_cast<int>(AppsListColumn::Icon);
    for (const auto &range : selectedRanges()) {
        for (int r = range.topRow(); r <= range.bottomRow(); ++r) {
            if (const auto *icon_item = dynamic_cast<const AppIconItem *>(item(r, icon_col))) {
                if (const auto *app = icon_item->app())
                    result.push_back(app);
            }
        }
    }
    return result;
}

void AppsListTable::mousePressEvent(QMouseEvent *event) {
    if (!indexAt(event->pos()).isValid())
        clearSelection();
    QTableWidget::mousePressEvent(event);

    const int row = currentRow();
    if (row < 0) {
        Q_EMIT app_selection_changed(nullptr);
        return;
    }
    if (const auto *icon_item = dynamic_cast<const AppIconItem *>(
            item(row, static_cast<int>(AppsListColumn::Icon)))) {
        Q_EMIT app_selection_changed(icon_item->app());
    }
}

void AppsListTable::mouseDoubleClickEvent(QMouseEvent *event) {
    QTableWidget::mouseDoubleClickEvent(event);
    const int row = currentRow();
    if (row >= 0)
        Q_EMIT app_boot_requested(app_from_row(row));
}

void AppsListTable::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const int row = currentRow();
        if (row >= 0)
            Q_EMIT app_boot_requested(app_from_row(row));
        return;
    }
    QTableWidget::keyPressEvent(event);
}

void AppsListTable::on_column_header_clicked(int col) {
    if (col == m_sort_column)
        m_sort_order = (m_sort_order == Qt::AscendingOrder)
            ? Qt::DescendingOrder
            : Qt::AscendingOrder;
    else
        m_sort_order = Qt::AscendingOrder;

    sort(col, m_sort_order);
}
