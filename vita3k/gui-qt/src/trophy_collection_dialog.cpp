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

#include <gui-qt/trophy_collection_dialog.h>

#include <config/state.h>
#include <gui-qt/gui_settings.h>
#include <io/state.h>
#include <np/state.h>
#include <util/fs.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QProgressDialog>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QtConcurrent>

static constexpr int RoleRawValue = Qt::UserRole + 1;
static constexpr int RoleGrade = Qt::UserRole + 2;
static constexpr int RoleHidden = Qt::UserRole + 3;
static constexpr int RoleEarned = Qt::UserRole + 4;
static constexpr int RoleTrophyId = Qt::UserRole + 5;
static constexpr int RoleTimestamp = Qt::UserRole + 6;
static constexpr int RoleOriginalPixmap = Qt::UserRole + 7;

namespace {

np::trophy::CollectionSource make_collection_source(const EmuEnvState &emuenv) {
    np::trophy::CollectionSource source;
    source.io = const_cast<IOState *>(&emuenv.io);
    source.vita_fs_path = emuenv.vita_fs_path;
    source.user_id = emuenv.io.user_id;
    source.lang = static_cast<uint32_t>(emuenv.cfg.sys_lang);
    return source;
}

class CenteredIconDelegate final : public QStyledItemDelegate {
public:
    explicit CenteredIconDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.state &= ~QStyle::State_HasFocus;
        opt.text.clear();
        opt.icon = QIcon();

        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        const QVariant decoration = index.data(Qt::DecorationRole);
        if (!decoration.canConvert<QPixmap>())
            return;

        const QPixmap pixmap = decoration.value<QPixmap>();
        if (pixmap.isNull())
            return;

        const QSizeF size = pixmap.deviceIndependentSize();
        const int x = opt.rect.x() + qRound((opt.rect.width() - size.width()) / 2.0);
        const int y = opt.rect.y() + qRound((opt.rect.height() - size.height()) / 2.0);
        painter->drawPixmap(x, y, pixmap);
    }
};

} // namespace

bool TrophyFilterProxy::filterAcceptsRow(int src_row, const QModelIndex &) const {
    const QStandardItemModel *m = qobject_cast<const QStandardItemModel *>(sourceModel());
    if (!m)
        return true;

    const bool earned = m->item(src_row, 0)->data(RoleEarned).toBool();
    const bool hidden = m->item(src_row, 0)->data(RoleHidden).toBool();
    const int grade = m->item(src_row, 0)->data(RoleGrade).toInt();

    if (earned && !show_unlocked)
        return false;
    if (!earned && !show_locked)
        return false;
    if (hidden && !earned && !show_hidden)
        return false;

    using G = np::trophy::SceNpTrophyGrade;
    switch (static_cast<G>(grade)) {
    case G::SCE_NP_TROPHY_GRADE_BRONZE: return show_bronze;
    case G::SCE_NP_TROPHY_GRADE_SILVER: return show_silver;
    case G::SCE_NP_TROPHY_GRADE_GOLD: return show_gold;
    case G::SCE_NP_TROPHY_GRADE_PLATINUM: return show_platinum;
    default: return true;
    }
}

TrophyCollectionDialog::TrophyCollectionDialog(EmuEnvState &emuenv,
    std::shared_ptr<GuiSettings> gui_settings)
    : QDialog(nullptr, Qt::Window)
    , m_emuenv(emuenv)
    , m_gui_settings(std::move(gui_settings)) {
    setWindowTitle(tr("Trophy Collection"));
    setObjectName(QStringLiteral("trophy_collection_dialog"));
    setWindowModality(Qt::NonModal);
    setWindowFlag(Qt::WindowSystemMenuHint, true);
    setWindowFlag(Qt::WindowMinMaxButtonsHint, true);
    setWindowFlag(Qt::WindowCloseButtonHint, true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(QStringLiteral(":/Vita3K.png")));

    m_app_model = new QStandardItemModel(0, AC_Count, this);
    m_app_model->setHorizontalHeaderLabels({ tr("Icon"), tr("App"), tr("Progress"), tr("Trophies") });

    m_app_table = new QTableView(this);
    m_app_table->setObjectName(QStringLiteral("trophy_app_table"));
    m_app_table->setModel(m_app_model);
    m_app_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_app_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_app_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_app_table->setFocusPolicy(Qt::NoFocus);
    m_app_table->setShowGrid(false);
    m_app_table->setAlternatingRowColors(false);
    m_app_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_app_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_app_table->verticalHeader()->setVisible(false);
    m_app_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_app_table->verticalHeader()->setDefaultSectionSize(default_app_row_height());
    m_app_table->horizontalHeader()->setHighlightSections(false);
    m_app_table->horizontalHeader()->setStretchLastSection(false);
    m_app_table->horizontalHeader()->setSectionResizeMode(AC_Icon, QHeaderView::Fixed);
    m_app_table->horizontalHeader()->setSectionResizeMode(AC_Name, QHeaderView::Stretch);
    m_app_table->horizontalHeader()->setSectionResizeMode(AC_Progress, QHeaderView::ResizeToContents);
    m_app_table->horizontalHeader()->setSectionResizeMode(AC_Trophies, QHeaderView::ResizeToContents);
    m_app_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_app_table->setItemDelegateForColumn(AC_Icon, new CenteredIconDelegate(m_app_table));

    m_trophy_model = new QStandardItemModel(0, TC_Count, this);
    m_trophy_model->setHorizontalHeaderLabels({ tr("Icon"), tr("Name"), tr("Description"),
        tr("Grade"), tr("Status"), tr("ID"), tr("Earned") });

    auto *proxy = new TrophyFilterProxy(this);
    proxy->setSourceModel(m_trophy_model);
    m_trophy_proxy = proxy;

    m_trophy_table = new QTableView(this);
    m_trophy_table->setObjectName(QStringLiteral("trophy_table"));
    m_trophy_table->setModel(m_trophy_proxy);
    m_trophy_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trophy_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_trophy_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_trophy_table->setFocusPolicy(Qt::NoFocus);
    m_trophy_table->setShowGrid(false);
    m_trophy_table->setAlternatingRowColors(false);
    m_trophy_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_trophy_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_trophy_table->verticalHeader()->setVisible(false);
    m_trophy_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_trophy_table->horizontalHeader()->setHighlightSections(false);
    m_trophy_table->horizontalHeader()->setStretchLastSection(false);
    m_trophy_table->horizontalHeader()->setSectionResizeMode(TC_Icon, QHeaderView::Fixed);
    m_trophy_table->horizontalHeader()->setSectionResizeMode(TC_Name, QHeaderView::Stretch);
    m_trophy_table->horizontalHeader()->setSectionResizeMode(TC_Detail, QHeaderView::Stretch);
    m_trophy_table->horizontalHeader()->setSectionResizeMode(TC_Grade, QHeaderView::ResizeToContents);
    m_trophy_table->horizontalHeader()->setSectionResizeMode(TC_Status, QHeaderView::ResizeToContents);
    m_trophy_table->horizontalHeader()->setSectionResizeMode(TC_Id, QHeaderView::ResizeToContents);
    m_trophy_table->horizontalHeader()->setSectionResizeMode(TC_Time, QHeaderView::ResizeToContents);
    m_trophy_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_trophy_table->setItemDelegateForColumn(TC_Icon, new CenteredIconDelegate(m_trophy_table));

    m_progress_label = new QLabel(tr("Progress: 0% (0/0)"), this);

    m_icon_slider = new QSlider(Qt::Horizontal, this);
    m_icon_slider->setRange(0, 100);
    m_icon_slider->setValue(m_app_slider_pos);
    m_icon_slider->setFixedWidth(100);

    const auto make_check = [](const QString &text, bool checked) {
        auto *cb = new QCheckBox(text);
        cb->setChecked(checked);
        return cb;
    };

    auto *filter_proxy = static_cast<TrophyFilterProxy *>(m_trophy_proxy);
    if (m_gui_settings) {
        filter_proxy->show_locked = m_gui_settings->get_value(gui::tr_showLocked).toBool();
        filter_proxy->show_unlocked = m_gui_settings->get_value(gui::tr_showUnlocked).toBool();
        filter_proxy->show_hidden = m_gui_settings->get_value(gui::tr_showHidden).toBool();
        filter_proxy->show_bronze = m_gui_settings->get_value(gui::tr_showBronze).toBool();
        filter_proxy->show_silver = m_gui_settings->get_value(gui::tr_showSilver).toBool();
        filter_proxy->show_gold = m_gui_settings->get_value(gui::tr_showGold).toBool();
        filter_proxy->show_platinum = m_gui_settings->get_value(gui::tr_showPlatinum).toBool();
    }
    auto *cb_locked = make_check(tr("Not Earned"), filter_proxy->show_locked);
    auto *cb_unlocked = make_check(tr("Earned"), filter_proxy->show_unlocked);
    auto *cb_hidden = make_check(tr("Hidden"), filter_proxy->show_hidden);
    auto *cb_bronze = make_check(tr("Bronze"), filter_proxy->show_bronze);
    auto *cb_silver = make_check(tr("Silver"), filter_proxy->show_silver);
    auto *cb_gold = make_check(tr("Gold"), filter_proxy->show_gold);
    auto *cb_platinum = make_check(tr("Platinum"), filter_proxy->show_platinum);

    m_trophy_filters_widget = new QWidget(this);
    auto *filter_bar = new QHBoxLayout(m_trophy_filters_widget);
    filter_bar->setContentsMargins(0, 0, 0, 0);
    filter_bar->setSpacing(6);
    filter_bar->addWidget(cb_locked);
    filter_bar->addWidget(cb_unlocked);
    filter_bar->addWidget(cb_hidden);
    filter_bar->addSpacing(8);
    filter_bar->addWidget(cb_bronze);
    filter_bar->addWidget(cb_silver);
    filter_bar->addWidget(cb_gold);
    filter_bar->addWidget(cb_platinum);

    auto *top_bar = new QHBoxLayout;
    top_bar->addWidget(m_progress_label);
    top_bar->addSpacing(12);
    top_bar->addWidget(m_trophy_filters_widget);
    top_bar->addStretch();
    top_bar->addWidget(new QLabel(tr("Icon size:"), this));
    top_bar->addWidget(m_icon_slider);

    m_stacked = new QStackedWidget(this);
    m_stacked->addWidget(m_app_table);

    m_back_button = new QPushButton(tr("\u2190 Back to Apps"), this);
    auto *trophy_page = new QWidget(this);
    auto *trophy_lay = new QVBoxLayout(trophy_page);
    trophy_lay->setContentsMargins(0, 0, 0, 0);
    trophy_lay->addWidget(m_back_button, 0, Qt::AlignLeft);
    trophy_lay->addWidget(m_trophy_table, 1);
    m_stacked->addWidget(trophy_page);

    auto *root = new QVBoxLayout(this);
    root->addLayout(top_bar);
    root->addWidget(m_stacked, 1);

    connect(m_app_table, &QTableView::doubleClicked,
        this, [this](const QModelIndex &idx) {
            if (!idx.isValid())
                return;
            m_selected_app_idx = m_app_model->item(idx.row(), AC_Name)
                                     ->data(RoleRawValue)
                                     .toInt();
            on_app_selection_changed(m_selected_app_idx);
            m_stacked->setCurrentIndex(1);
        });

    connect(m_back_button, &QPushButton::clicked, this, [this] {
        m_stacked->setCurrentIndex(0);
    });

    connect(m_stacked, &QStackedWidget::currentChanged, this, [this](int idx) {
        const QSignalBlocker blocker(m_icon_slider);
        m_icon_slider->setValue(idx == 0 ? m_app_slider_pos : m_trophy_slider_pos);
        if (idx == 0)
            update_summary_progress();
        update_page_controls();
    });

    connect(m_icon_slider, &QSlider::valueChanged, this, [this](int val) {
        const int sz = 48 + (val * (160 - 48)) / 100;
        if (m_stacked->currentIndex() == 0) {
            m_app_icon_size = QSize(sz, sz);
            m_app_slider_pos = val;
            adjust_app_icon_column();
            repaint_app_icons();
        } else {
            m_trophy_icon_size = QSize(sz, sz);
            m_trophy_slider_pos = val;
            adjust_trophy_icon_column();
            repaint_trophy_icons();
        }
    });

    const auto wire_check = [&](QCheckBox *cb, bool &flag) {
        connect(cb, &QCheckBox::toggled, this, [this, &flag](bool v) {
            flag = v;
            apply_filter();
        });
    };
    wire_check(cb_locked, filter_proxy->show_locked);
    wire_check(cb_unlocked, filter_proxy->show_unlocked);
    wire_check(cb_hidden, filter_proxy->show_hidden);
    wire_check(cb_bronze, filter_proxy->show_bronze);
    wire_check(cb_silver, filter_proxy->show_silver);
    wire_check(cb_gold, filter_proxy->show_gold);
    wire_check(cb_platinum, filter_proxy->show_platinum);

    connect(m_trophy_table, &QTableView::customContextMenuRequested,
        this, &TrophyCollectionDialog::show_trophy_context_menu);
    connect(m_app_table, &QTableView::customContextMenuRequested,
        this, &TrophyCollectionDialog::show_app_context_menu);

    restore_persistent_state();
    load_all_apps();
    update_page_controls();

    m_trophy_cb_alive = std::make_shared<bool>(true);
    m_emuenv.np.trophy_state.add_trophy_unlock_callback(
        [weak_alive = std::weak_ptr<bool>(m_trophy_cb_alive), this](NpTrophyUnlockCallbackData &data) {
            if (weak_alive.expired())
                return;
            QString np_com_id = QString::fromStdString(data.np_com_id);
            QMetaObject::invokeMethod(
                this, [this, id = std::move(np_com_id)]() { on_trophy_unlocked(id); }, Qt::QueuedConnection);
        });
}

TrophyCollectionDialog::~TrophyCollectionDialog() {
    m_trophy_cb_alive.reset();
}

void TrophyCollectionDialog::load_all_apps() {
    m_db.clear();

    const auto collection_ids = np::trophy::list_collection_ids(make_collection_source(m_emuenv));
    if (collection_ids.empty()) {
        populate_app_table();
        return;
    }

    QProgressDialog progress(tr("Loading trophies…"), tr("Cancel"), 0, static_cast<int>(collection_ids.size()), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    for (int i = 0; i < static_cast<int>(collection_ids.size()); ++i) {
        if (progress.wasCanceled())
            break;
        load_app_data(QString::fromStdString(collection_ids[i]));
        progress.setValue(i + 1);
    }

    populate_app_table();
}

bool TrophyCollectionDialog::load_app_data(const QString &np_com_id_str) {
    auto data = load_app_data_entry(np_com_id_str);
    if (!data)
        return false;
    m_db.push_back(std::move(data));
    return true;
}

std::unique_ptr<TrophyAppData> TrophyCollectionDialog::load_app_data_entry(const QString &np_com_id_str) const {
    auto data = std::make_unique<TrophyAppData>();
    if (!np::trophy::load_collection(make_collection_source(m_emuenv), np_com_id_str.toStdString(), *data))
        return nullptr;
    return data;
}

void TrophyCollectionDialog::populate_app_table() {
    m_app_model->removeRows(0, m_app_model->rowCount());

    std::sort(m_db.begin(), m_db.end(), [](const auto &a, const auto &b) {
        return QString::fromStdString(a->title).toLower() < QString::fromStdString(b->title).toLower();
    });

    for (int i = 0; i < static_cast<int>(m_db.size()); ++i) {
        const auto &g = *m_db[i];
        const int pct = g.total > 0 ? (g.unlocked * 100 / g.total) : 0;

        auto *icon_item = new QStandardItem;
        auto *name_item = new QStandardItem(QString::fromStdString(g.title));
        auto *progress_item = new QStandardItem(tr("%1% (%2/%3)").arg(pct).arg(g.unlocked).arg(g.total));
        auto *trophy_item = new QStandardItem(QString::number(g.total));

        name_item->setData(i, RoleRawValue);
        progress_item->setData(pct, RoleRawValue);
        trophy_item->setData(g.total, RoleRawValue);

        m_app_model->appendRow({ icon_item, name_item, progress_item, trophy_item });

        load_app_icon_async(i, QString::fromUtf8(g.icon_path.c_str()));
    }

    adjust_app_icon_column();
    if (!m_has_saved_app_header_state)
        initialize_app_column_layout();
    update_summary_progress();
}

void TrophyCollectionDialog::populate_trophy_table(int app_idx) {
    m_trophy_model->removeRows(0, m_trophy_model->rowCount());

    if (app_idx < 0 || app_idx >= static_cast<int>(m_db.size()))
        return;

    const auto &g = *m_db[app_idx];
    const int pct = g.total > 0 ? (g.unlocked * 100 / g.total) : 0;
    m_progress_label->setText(tr("Progress: %1% (%2/%3)").arg(pct).arg(g.unlocked).arg(g.total));

    using G = np::trophy::SceNpTrophyGrade;
    const auto grade_str = [](int grade) -> QString {
        switch (static_cast<G>(grade)) {
        case G::SCE_NP_TROPHY_GRADE_PLATINUM: return QObject::tr("Platinum");
        case G::SCE_NP_TROPHY_GRADE_GOLD: return QObject::tr("Gold");
        case G::SCE_NP_TROPHY_GRADE_SILVER: return QObject::tr("Silver");
        case G::SCE_NP_TROPHY_GRADE_BRONZE: return QObject::tr("Bronze");
        default: return {};
        }
    };

    for (int row = 0; row < static_cast<int>(g.trophies.size()); ++row) {
        const auto &t = g.trophies[row];

        auto *icon_item = new QStandardItem;
        auto *name_item = new QStandardItem(QString::fromStdString(t.name));
        auto *detail_item = new QStandardItem(QString::fromStdString(t.detail));
        auto *grade_item = new QStandardItem(grade_str(t.grade));
        auto *status_item = new QStandardItem(t.earned ? tr("Earned") : tr("Not Earned"));
        auto *id_item = new QStandardItem(QString::number(t.id));
        auto *time_item = new QStandardItem(t.earned ? format_timestamp(t.timestamp) : tr("—"));

        icon_item->setData(t.hidden, RoleHidden);
        icon_item->setData(t.earned, RoleEarned);
        icon_item->setData(t.grade, RoleGrade);
        icon_item->setData(t.id, RoleTrophyId);
        id_item->setData(t.id, RoleRawValue);
        time_item->setData(QVariant::fromValue<quint64>(t.timestamp), RoleTimestamp);
        grade_item->setData(t.grade, RoleRawValue);

        m_trophy_model->appendRow({ icon_item, name_item, detail_item,
            grade_item, status_item, id_item, time_item });

        load_trophy_icon_async(row, QString::fromUtf8(t.icon_path.c_str()));
    }

    adjust_trophy_icon_column();
    if (!m_has_saved_trophy_header_state)
        initialize_trophy_column_layout();
    apply_filter();
}

void TrophyCollectionDialog::load_icon_async(QStandardItemModel *model, int column, const QSize &icon_size, int row, const QString &path) {
    auto *watcher = new QFutureWatcher<QPixmap>(this);
    connect(watcher, &QFutureWatcher<QPixmap>::finished, this, [this, model, column, icon_size, row, watcher] {
        if (auto *item = model->item(row, column)) {
            const QPixmap orig = watcher->result();
            item->setData(orig, RoleOriginalPixmap);
            item->setData(orig.scaled(icon_size, Qt::KeepAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            if (model == m_app_model)
                update_app_row_height(row);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([path] {
        QPixmap px;
        px.load(path);
        return px;
    }));
}

void TrophyCollectionDialog::load_app_icon_async(int row, const QString &path) {
    load_icon_async(m_app_model, AC_Icon, m_app_icon_size, row, path);
}

void TrophyCollectionDialog::load_trophy_icon_async(int row, const QString &path) {
    load_icon_async(m_trophy_model, TC_Icon, m_trophy_icon_size, row, path);
}

void TrophyCollectionDialog::apply_filter() {
    auto *proxy = static_cast<TrophyFilterProxy *>(m_trophy_proxy);
    proxy->invalidate();
}

void TrophyCollectionDialog::adjust_trophy_icon_column() {
    const int h = m_trophy_icon_size.height();
    const int w = m_trophy_icon_size.width();
    m_trophy_table->verticalHeader()->setDefaultSectionSize(h);
    m_trophy_table->verticalHeader()->setMinimumSectionSize(h);
    m_trophy_table->verticalHeader()->setMaximumSectionSize(h);
    m_trophy_table->horizontalHeader()->resizeSection(TC_Icon, w);
    update_trophy_column_layout();
}

int TrophyCollectionDialog::default_app_row_height() const {
    return std::max(24, qRound(m_app_icon_size.width() * 9.0 / 16.0) + 2);
}

void TrophyCollectionDialog::update_app_row_height(int row) {
    if (row < 0 || row >= m_app_model->rowCount())
        return;

    int row_height = default_app_row_height();
    if (auto *item = m_app_model->item(row, AC_Icon)) {
        const QVariant original = item->data(RoleOriginalPixmap);
        if (original.canConvert<QPixmap>()) {
            const QPixmap pixmap = original.value<QPixmap>();
            if (!pixmap.isNull()) {
                row_height = pixmap.scaled(m_app_icon_size, Qt::KeepAspectRatio, Qt::SmoothTransformation).height() + 2;
            }
        }
    }

    m_app_table->setRowHeight(row, row_height);
}

void TrophyCollectionDialog::adjust_app_icon_column() {
    m_app_table->verticalHeader()->setDefaultSectionSize(default_app_row_height());
    m_app_table->horizontalHeader()->resizeSection(AC_Icon, m_app_icon_size.width());
    for (int row = 0; row < m_app_model->rowCount(); ++row)
        update_app_row_height(row);
    update_app_column_layout();
}

static void repaint_icons(QStandardItemModel *model, int column, const QSize &icon_size) {
    for (int r = 0; r < model->rowCount(); ++r) {
        auto *item = model->item(r, column);
        if (!item)
            continue;
        const QVariant v = item->data(RoleOriginalPixmap);
        if (!v.canConvert<QPixmap>())
            continue;
        const QPixmap orig = v.value<QPixmap>();
        if (orig.isNull())
            continue;
        item->setData(orig.scaled(icon_size, Qt::KeepAspectRatio, Qt::SmoothTransformation),
            Qt::DecorationRole);
    }
}

void TrophyCollectionDialog::repaint_trophy_icons() {
    repaint_icons(m_trophy_model, TC_Icon, m_trophy_icon_size);
}

void TrophyCollectionDialog::repaint_app_icons() {
    repaint_icons(m_app_model, AC_Icon, m_app_icon_size);
    for (int row = 0; row < m_app_model->rowCount(); ++row)
        update_app_row_height(row);
}

void TrophyCollectionDialog::update_summary_progress() {
    int total_unlocked = 0;
    int total_trophies = 0;
    for (const auto &g : m_db) {
        total_unlocked += g->unlocked;
        total_trophies += g->total;
    }
    const int pct = total_trophies > 0 ? (total_unlocked * 100 / total_trophies) : 0;
    m_progress_label->setText(tr("Total: %1% (%2/%3 trophies across %4 apps)")
            .arg(pct)
            .arg(total_unlocked)
            .arg(total_trophies)
            .arg(m_db.size()));
}

void TrophyCollectionDialog::update_page_controls() {
    const bool trophy_view = m_stacked && (m_stacked->currentIndex() == 1);
    if (m_trophy_filters_widget)
        m_trophy_filters_widget->setVisible(trophy_view);
}

void TrophyCollectionDialog::update_app_column_layout() {
    if (!m_app_table)
        return;

    if (!m_has_saved_app_header_state) {
        m_app_table->resizeColumnToContents(AC_Progress);
        m_app_table->resizeColumnToContents(AC_Trophies);
    }

    m_app_table->horizontalHeader()->resizeSection(AC_Icon, m_app_icon_size.width());
}

void TrophyCollectionDialog::update_trophy_column_layout() {
    if (!m_trophy_table)
        return;

    if (!m_has_saved_trophy_header_state) {
        m_trophy_table->resizeColumnToContents(TC_Grade);
        m_trophy_table->resizeColumnToContents(TC_Status);
        m_trophy_table->resizeColumnToContents(TC_Id);
        m_trophy_table->resizeColumnToContents(TC_Time);
    }

    m_trophy_table->horizontalHeader()->resizeSection(TC_Icon, m_trophy_icon_size.width());
}

void TrophyCollectionDialog::enable_app_column_resizing() {
    auto *header = m_app_table ? m_app_table->horizontalHeader() : nullptr;
    if (!header)
        return;

    header->setSectionResizeMode(AC_Icon, QHeaderView::Fixed);
    header->setSectionResizeMode(AC_Name, QHeaderView::Interactive);
    header->setSectionResizeMode(AC_Progress, QHeaderView::Interactive);
    header->setSectionResizeMode(AC_Trophies, QHeaderView::Interactive);
}

void TrophyCollectionDialog::enable_trophy_column_resizing() {
    auto *header = m_trophy_table ? m_trophy_table->horizontalHeader() : nullptr;
    if (!header)
        return;

    header->setSectionResizeMode(TC_Icon, QHeaderView::Fixed);
    header->setSectionResizeMode(TC_Name, QHeaderView::Interactive);
    header->setSectionResizeMode(TC_Detail, QHeaderView::Interactive);
    header->setSectionResizeMode(TC_Grade, QHeaderView::Interactive);
    header->setSectionResizeMode(TC_Status, QHeaderView::Interactive);
    header->setSectionResizeMode(TC_Id, QHeaderView::Interactive);
    header->setSectionResizeMode(TC_Time, QHeaderView::Interactive);
}

void TrophyCollectionDialog::initialize_app_column_layout() {
    if (!m_app_table)
        return;

    update_app_column_layout();
}

void TrophyCollectionDialog::initialize_trophy_column_layout() {
    if (!m_trophy_table)
        return;

    update_trophy_column_layout();
}

void TrophyCollectionDialog::on_app_selection_changed(int app_idx) {
    populate_trophy_table(app_idx);
}

void TrophyCollectionDialog::show_trophy_context_menu(const QPoint &pos) {
    const QModelIndex proxy_idx = m_trophy_table->indexAt(pos);
    if (!proxy_idx.isValid())
        return;
    const QModelIndex src_idx = m_trophy_proxy->mapToSource(proxy_idx);

    const int trophy_id = m_trophy_model->item(src_idx.row(), TC_Icon)->data(RoleTrophyId).toInt();
    const bool earned = m_trophy_model->item(src_idx.row(), TC_Icon)->data(RoleEarned).toBool();
    const int grade = m_trophy_model->item(src_idx.row(), TC_Icon)->data(RoleGrade).toInt();

    const int db_idx = m_selected_app_idx;
    if (db_idx < 0 || db_idx >= static_cast<int>(m_db.size()))
        return;

    QMenu menu(this);

    const QString name = m_trophy_model->item(src_idx.row(), TC_Name)->text();
    const QString detail = m_trophy_model->item(src_idx.row(), TC_Detail)->text();

    if (!name.isEmpty() || !detail.isEmpty()) {
        QMenu *copy_menu = menu.addMenu(tr("&Copy Info"));
        if (!name.isEmpty() && !detail.isEmpty())
            connect(copy_menu->addAction(tr("Name + Description")), &QAction::triggered, this,
                [name, detail] { QApplication::clipboard()->setText(name + "\n\n" + detail); });
        if (!name.isEmpty())
            connect(copy_menu->addAction(tr("Name")), &QAction::triggered, this,
                [name] { QApplication::clipboard()->setText(name); });
        if (!detail.isEmpty())
            connect(copy_menu->addAction(tr("Description")), &QAction::triggered, this,
                [detail] { QApplication::clipboard()->setText(detail); });
    }

    const bool game_running = !m_emuenv.io.app_path.empty();
    if (!game_running) {
        using G = np::trophy::SceNpTrophyGrade;
        const bool is_platinum = static_cast<G>(grade) == G::SCE_NP_TROPHY_GRADE_PLATINUM;
        menu.addSeparator();
        auto *lock_act = menu.addAction(earned ? tr("&Lock Trophy") : tr("&Unlock Trophy"));
        connect(lock_act, &QAction::triggered, this, [=, this] {
            if (is_platinum && !earned) {
                QMessageBox::information(this, tr("Not permitted"),
                    tr("Platinum trophies can only be unlocked in-game."));
                return;
            }
            auto &game = *m_db[db_idx];
            auto &ctx = game.context;
            auto *trophy_entry = [&]() -> TrophyEntry * {
                for (auto &t : game.trophies)
                    if (t.id == trophy_id)
                        return &t;
                return nullptr;
            }();
            if (!trophy_entry)
                return;
            np::NpTrophyError err;
            if (earned) {
                ctx.trophy_progress[trophy_id >> 5] &= ~(1u << (trophy_id & 31));
                ctx.unlock_timestamps[trophy_id] = 0;
                ctx.save_trophy_progress_file();
                trophy_entry->earned = false;
                trophy_entry->timestamp = 0;
                game.unlocked--;
                game.grade_unlocked[grade]--;
            } else {
                ctx.unlock_trophy(trophy_id, &err, false);
                trophy_entry->earned = true;
                trophy_entry->timestamp = ctx.unlock_timestamps[trophy_id];
                game.unlocked++;
                game.grade_unlocked[grade]++;
            }
            populate_trophy_table(db_idx);
        });
    }

    menu.exec(m_trophy_table->viewport()->mapToGlobal(pos));
}

void TrophyCollectionDialog::show_app_context_menu(const QPoint &pos) {
    const QModelIndex idx = m_app_table->indexAt(pos);
    if (!idx.isValid())
        return;

    const int db_idx = m_app_model->item(idx.row(), AC_Name)->data(RoleRawValue).toInt();
    if (db_idx < 0 || db_idx >= static_cast<int>(m_db.size()))
        return;

    const QString name = QString::fromStdString(m_db[db_idx]->title);
    const fs::path conf = m_emuenv.vita_fs_path / "ux0/user"
        / m_emuenv.io.user_id / "trophy/conf" / m_db[db_idx]->np_com_id;
    const fs::path data_dir = m_emuenv.vita_fs_path / "ux0/user"
        / m_emuenv.io.user_id / "trophy/data" / m_db[db_idx]->np_com_id;

    QMenu menu(this);
    connect(menu.addAction(tr("&Remove")), &QAction::triggered, this, [=, this] {
        if (QMessageBox::question(this, tr("Confirm delete"),
                tr("Delete all trophies for:\n%1?").arg(name),
                QMessageBox::Yes | QMessageBox::No)
            != QMessageBox::Yes)
            return;
        fs::remove_all(conf);
        fs::remove_all(data_dir);
        m_db.erase(m_db.begin() + db_idx);
        m_selected_app_idx = -1;
        populate_app_table();
    });

    menu.exec(m_app_table->viewport()->mapToGlobal(pos));
}

void TrophyCollectionDialog::jump_to_trophy(const QString &np_com_id, int trophy_id) {
    for (int i = 0; i < static_cast<int>(m_db.size()); ++i) {
        if (QString::fromStdString(m_db[i]->np_com_id) != np_com_id)
            continue;
        m_app_table->selectRow(i);
        m_selected_app_idx = i;
        on_app_selection_changed(i);
        m_stacked->setCurrentIndex(1);
        for (int row = 0; row < m_trophy_proxy->rowCount(); ++row) {
            const QModelIndex pi = m_trophy_proxy->index(row, TC_Icon);
            const QModelIndex src = m_trophy_proxy->mapToSource(pi);
            if (m_trophy_model->item(src.row(), TC_Icon)->data(RoleTrophyId).toInt() == trophy_id) {
                m_trophy_table->selectRow(row);
                m_trophy_table->scrollTo(pi);
                break;
            }
        }
        break;
    }
    raise();
    activateWindow();
}

void TrophyCollectionDialog::refresh_app(const QString &np_com_id) {
    for (int i = 0; i < static_cast<int>(m_db.size()); ++i) {
        if (QString::fromStdString(m_db[i]->np_com_id) != np_com_id)
            continue;

        auto refreshed = load_app_data_entry(np_com_id);
        if (!refreshed)
            break;

        m_db[i] = std::move(refreshed);
        const bool trophy_view = m_stacked && (m_stacked->currentIndex() == 1);
        populate_app_table();

        if (m_selected_app_idx == i) {
            m_app_table->selectRow(i);
            populate_trophy_table(i);
            if (trophy_view)
                m_stacked->setCurrentIndex(1);
        }
        break;
    }
}

void TrophyCollectionDialog::on_trophy_unlocked(const QString &np_com_id) {
    if (isVisible())
        refresh_app(np_com_id);
}

void TrophyCollectionDialog::reload() {
    m_selected_app_idx = -1;
    m_stacked->setCurrentIndex(0);
    load_all_apps();
}

QString TrophyCollectionDialog::format_timestamp(quint64 unix_sec) {
    if (!unix_sec)
        return tr("Unknown");

    return QLocale().toString(
        QDateTime::fromSecsSinceEpoch(static_cast<qint64>(unix_sec)),
        QLocale::LongFormat);
}

void TrophyCollectionDialog::closeEvent(QCloseEvent *event) {
    save_persistent_state();
    QDialog::closeEvent(event);
}

void TrophyCollectionDialog::restore_persistent_state() {
    if (!m_gui_settings) {
        resize(1024, 640);
        return;
    }

    if (!restoreGeometry(m_gui_settings->get_value(gui::tr_geometry).toByteArray()))
        resize(1024, 640);

    const QByteArray app_header_state = m_gui_settings->get_value(gui::tr_appHeaderState).toByteArray();
    if (!app_header_state.isEmpty()) {
        m_app_table->horizontalHeader()->restoreState(app_header_state);
        enable_app_column_resizing();
        m_has_saved_app_header_state = true;
    }

    const QByteArray trophy_header_state = m_gui_settings->get_value(gui::tr_trophyHeaderState).toByteArray();
    if (!trophy_header_state.isEmpty()) {
        m_trophy_table->horizontalHeader()->restoreState(trophy_header_state);
        enable_trophy_column_resizing();
        m_has_saved_trophy_header_state = true;
    }
}

void TrophyCollectionDialog::save_persistent_state() const {
    if (!m_gui_settings)
        return;

    auto *filter_proxy = static_cast<TrophyFilterProxy *>(m_trophy_proxy);
    m_gui_settings->set_value(gui::tr_geometry, saveGeometry(), false);
    m_gui_settings->set_value(gui::tr_appHeaderState, m_app_table->horizontalHeader()->saveState(), false);
    m_gui_settings->set_value(gui::tr_trophyHeaderState, m_trophy_table->horizontalHeader()->saveState(), false);
    m_gui_settings->set_value(gui::tr_showLocked, filter_proxy->show_locked, false);
    m_gui_settings->set_value(gui::tr_showUnlocked, filter_proxy->show_unlocked, false);
    m_gui_settings->set_value(gui::tr_showHidden, filter_proxy->show_hidden, false);
    m_gui_settings->set_value(gui::tr_showBronze, filter_proxy->show_bronze, false);
    m_gui_settings->set_value(gui::tr_showSilver, filter_proxy->show_silver, false);
    m_gui_settings->set_value(gui::tr_showGold, filter_proxy->show_gold, false);
    m_gui_settings->set_value(gui::tr_showPlatinum, filter_proxy->show_platinum, false);
    m_gui_settings->sync();
}

void TrophyCollectionDialog::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    update_app_column_layout();
    update_trophy_column_layout();
}
