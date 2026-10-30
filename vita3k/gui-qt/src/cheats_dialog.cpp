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

#include <gui-qt/cheats_dialog.h>

#include <gui-qt/gui_settings.h>
#include <gui-qt/qt_utils.h>

#include <app/functions.h>
#include <cheat/functions.h>
#include <config/functions.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <kernel/state.h>
#include <util/string_utils.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>
#include <map>
#include <set>

namespace {

enum TreeColumn {
    COLUMN_NAME,
    COLUMN_CODES,
    COLUMN_ON_BOOT,
    COLUMN_COUNT,
};

enum ResultColumn {
    RESULT_ADDRESS,
    RESULT_VALUE,
    RESULT_COUNT,
};

constexpr int game_role = Qt::UserRole;
constexpr int cheat_role = Qt::UserRole + 1;

// Filling the table with millions of rows would freeze the dialog, the rest stays searchable.
constexpr int max_shown_results = 1000;

QString format_address(uint32_t address) {
    return QStringLiteral("0x") + QStringLiteral("%1").arg(address, 8, 16, QLatin1Char('0')).toUpper();
}

} // namespace

CheatsDialog::CheatsDialog(EmuEnvState &emuenv, std::shared_ptr<GuiSettings> gui_settings, QWidget *parent)
    : QDialog(parent)
    , emuenv(emuenv)
    , m_gui_settings(std::move(gui_settings)) {
    setWindowTitle(tr("Cheat Manager"));
    setMinimumSize(640, 620);
    resize(860, 900);

    auto *layout = new QVBoxLayout(this);

    m_master = new QCheckBox(tr("Enable cheats"), this);
    m_master->setToolTip(tr("Master switch. When it is off, no cheat is applied to any game."));
    m_master->setChecked(emuenv.cfg.enable_cheats);

    m_installed_only = new QCheckBox(tr("Installed games only"), this);
    m_installed_only->setToolTip(tr("Hide the games of the cheat databases that are not in the app list."));
    m_installed_only->setChecked(m_gui_settings && m_gui_settings->get_value(gui::ch_installedOnly).toBool());

    auto *top_layout = new QHBoxLayout;
    top_layout->addWidget(m_master);
    top_layout->addSpacing(16);
    top_layout->addWidget(m_installed_only);
    top_layout->addStretch();
    layout->addLayout(top_layout);

    m_enable_all_button = new QPushButton(tr("Enable All"), this);
    m_enable_all_button->setToolTip(tr("Turn on every cheat of the selected game."));
    m_disable_all_button = new QPushButton(tr("Disable All"), this);
    m_disable_all_button->setToolTip(tr("Turn off every cheat of the selected game."));

    auto *filter_layout = new QHBoxLayout;
    filter_layout->addWidget(new QLabel(tr("Search:"), this));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Filter by game, title ID or cheat name"));
    m_filter->setClearButtonEnabled(true);
    filter_layout->addWidget(m_filter);
    filter_layout->addWidget(m_enable_all_button);
    filter_layout->addWidget(m_disable_all_button);
    layout->addLayout(filter_layout);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(COLUMN_COUNT);
    m_tree->setHeaderLabels({ tr("Game / Cheat"), tr("Codes"), tr("On boot") });
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(COLUMN_NAME, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(COLUMN_CODES, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COLUMN_ON_BOOT, QHeaderView::ResizeToContents);

    m_details = new QPlainTextEdit(this);
    m_details->setReadOnly(true);
    m_details->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_details->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_details->setPlaceholderText(tr("Select a cheat to see its codes"));

    auto *cheats_splitter = new QSplitter(Qt::Vertical, this);
    cheats_splitter->addWidget(m_tree);
    cheats_splitter->addWidget(m_details);
    cheats_splitter->setStretchFactor(0, 4);
    cheats_splitter->setStretchFactor(1, 1);
    cheats_splitter->setSizes({ 320, 90 });

    auto *cheats_widget = new QWidget(this);
    auto *cheats_layout = new QVBoxLayout(cheats_widget);
    cheats_layout->setContentsMargins(0, 0, 0, 0);
    cheats_layout->addWidget(cheats_splitter, 1);

    m_status = new QLabel(cheats_widget);
    m_status->setWordWrap(true);
    cheats_layout->addWidget(m_status);

    auto *button_layout = new QHBoxLayout;
    auto *reload_button = new QPushButton(tr("Reload"), cheats_widget);
    reload_button->setToolTip(tr("Read the cheats folder again, for instance after editing a cheat file."));
    m_save_button = new QPushButton(tr("Save"), cheats_widget);
    m_save_button->setToolTip(tr("Write the cheats of the selected game that are on back to its cheat file, so that they are on again next time it boots."));
    m_open_file_button = new QPushButton(tr("Open Cheat File"), cheats_widget);
    m_open_file_button->setToolTip(tr("Open the cheat file of the selected game in the default editor."));
    auto *open_folder_button = new QPushButton(tr("Open Cheats Folder"), cheats_widget);
    button_layout->addWidget(reload_button);
    button_layout->addWidget(m_save_button);
    button_layout->addWidget(m_open_file_button);
    button_layout->addWidget(open_folder_button);
    button_layout->addStretch();
    cheats_layout->addLayout(button_layout);

    m_search_group = new QGroupBox(tr("Cheat Search"), this);
    auto *search_layout = new QVBoxLayout(m_search_group);

    m_search_hint = new QLabel(m_search_group);
    m_search_hint->setWordWrap(true);
    search_layout->addWidget(m_search_hint);

    m_new_search_button = new QPushButton(tr("New Search"), m_search_group);
    m_new_search_button->setToolTip(tr("Look for the value in the whole memory of the running game."));
    m_filter_results_button = new QPushButton(tr("Filter Results"), m_search_group);
    m_filter_results_button->setToolTip(tr("Keep only the addresses that hold the value now. Change the value in game, then filter again."));
    m_search_value = new QLineEdit(m_search_group);
    m_search_value->setPlaceholderText(tr("Value, decimal or 0x hexadecimal"));
    m_search_type = new QComboBox(m_search_group);
    m_search_type->addItem(tr("Unsigned 8 bits"), static_cast<int>(cheat::SearchType::u8));
    m_search_type->addItem(tr("Signed 8 bits"), static_cast<int>(cheat::SearchType::s8));
    m_search_type->addItem(tr("Unsigned 16 bits"), static_cast<int>(cheat::SearchType::u16));
    m_search_type->addItem(tr("Signed 16 bits"), static_cast<int>(cheat::SearchType::s16));
    m_search_type->addItem(tr("Unsigned 32 bits"), static_cast<int>(cheat::SearchType::u32));
    m_search_type->addItem(tr("Signed 32 bits"), static_cast<int>(cheat::SearchType::s32));
    m_search_type->addItem(tr("Float 32 bits"), static_cast<int>(cheat::SearchType::f32));
    m_search_type->setCurrentIndex(m_search_type->findData(static_cast<int>(cheat::SearchType::s32)));

    auto *search_row = new QHBoxLayout;
    search_row->addWidget(m_new_search_button);
    search_row->addWidget(m_filter_results_button);
    search_row->addWidget(m_search_value, 1);
    search_row->addWidget(m_search_type);
    search_layout->addLayout(search_row);

    m_results = new QTableWidget(m_search_group);
    m_results->setColumnCount(RESULT_COUNT);
    m_results->setHorizontalHeaderLabels({ tr("Address"), tr("Current value") });
    m_results->verticalHeader()->setVisible(false);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setContextMenuPolicy(Qt::CustomContextMenu);
    m_results->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_results->horizontalHeader()->setSectionResizeMode(RESULT_ADDRESS, QHeaderView::ResizeToContents);
    m_results->horizontalHeader()->setSectionResizeMode(RESULT_VALUE, QHeaderView::Stretch);
    m_results->setMinimumHeight(140);
    search_layout->addWidget(m_results, 1);

    m_results_label = new QLabel(m_search_group);
    m_results_label->setWordWrap(true);
    search_layout->addWidget(m_results_label);

    auto *value_row = new QHBoxLayout;
    value_row->addWidget(new QLabel(tr("Current value:"), m_search_group));
    m_current_value = new QLineEdit(m_search_group);
    m_current_value->setPlaceholderText(tr("Select a result"));
    value_row->addWidget(m_current_value, 1);
    m_apply_button = new QPushButton(tr("Apply"), m_search_group);
    m_apply_button->setToolTip(tr("Write this value to the selected address once."));
    value_row->addWidget(m_apply_button);
    m_add_cheat_button = new QPushButton(tr("Add as Cheat..."), m_search_group);
    m_add_cheat_button->setToolTip(tr("Add a cheat to the cheat file of the game that keeps the selected address at this value."));
    value_row->addWidget(m_add_cheat_button);
    search_layout->addLayout(value_row);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(cheats_widget);
    splitter->addWidget(m_search_group);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttons);

    m_refresh_timer = new QTimer(this);
    m_refresh_timer->setInterval(500);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(m_master, &QCheckBox::toggled, this, &CheatsDialog::on_master_toggled);
    connect(m_filter, &QLineEdit::textChanged, this, [this] { apply_filter(); });
    connect(m_installed_only, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_gui_settings)
            m_gui_settings->set_value(gui::ch_installedOnly, checked);
        apply_filter();
        update_status();
    });
    connect(m_tree, &QTreeWidget::itemChanged, this, &CheatsDialog::on_item_changed);
    connect(m_tree, &QTreeWidget::itemExpanded, this, &CheatsDialog::on_item_expanded);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &CheatsDialog::on_selection_changed);
    connect(m_enable_all_button, &QPushButton::clicked, this, [this] { set_all_enabled(true); });
    connect(m_disable_all_button, &QPushButton::clicked, this, [this] { set_all_enabled(false); });
    connect(reload_button, &QPushButton::clicked, this, &CheatsDialog::on_reload);
    connect(m_save_button, &QPushButton::clicked, this, &CheatsDialog::on_save);
    connect(m_open_file_button, &QPushButton::clicked, this, &CheatsDialog::on_open_file);
    connect(open_folder_button, &QPushButton::clicked, this, [this] { gui::utils::open_dir(this->emuenv.cheat_path); });
    connect(m_new_search_button, &QPushButton::clicked, this, &CheatsDialog::on_new_search);
    connect(m_filter_results_button, &QPushButton::clicked, this, &CheatsDialog::on_filter_results);
    connect(m_search_value, &QLineEdit::returnPressed, this, [this] {
        if (m_search.has_searched())
            on_filter_results();
        else
            on_new_search();
    });
    // Results of one width mean nothing for another, so a new type starts over.
    connect(m_search_type, &QComboBox::currentIndexChanged, this, [this] {
        m_search.clear();
        show_results();
    });
    connect(m_results, &QTableWidget::itemSelectionChanged, this, &CheatsDialog::on_result_selected);
    connect(m_results, &QTableWidget::customContextMenuRequested, this, &CheatsDialog::on_results_context_menu);
    connect(m_results, &QTableWidget::cellDoubleClicked, this, [this] { on_add_cheat(); });
    connect(m_apply_button, &QPushButton::clicked, this, &CheatsDialog::on_apply_value);
    connect(m_current_value, &QLineEdit::returnPressed, this, &CheatsDialog::on_apply_value);
    connect(m_add_cheat_button, &QPushButton::clicked, this, &CheatsDialog::on_add_cheat);
    connect(m_refresh_timer, &QTimer::timeout, this, &CheatsDialog::refresh_result_values);

    reload();
}

std::string CheatsDialog::running_title_id() const {
    return cheat::loaded_title_id(emuenv.cheat);
}

// io.title_id is also set by the live area of a game that is not running.
bool CheatsDialog::is_live(const Game &game) const {
    return game.info.title_id == running_title_id();
}

CheatsDialog::Game *CheatsDialog::find_game(const std::string &title_id) {
    const auto it = std::ranges::find_if(m_games, [&](const Game &game) { return game.info.title_id == title_id; });
    return (it == m_games.end()) ? nullptr : &*it;
}

CheatsDialog::Game *CheatsDialog::selected_game() {
    const auto *item = m_tree->currentItem();
    if (!item)
        return nullptr;

    const auto index = item->data(COLUMN_NAME, game_role).toULongLong();
    return (index < m_games.size()) ? &m_games[index] : nullptr;
}

std::optional<size_t> CheatsDialog::selected_cheat() const {
    const auto *item = m_tree->currentItem();
    if (!item || !item->parent())
        return std::nullopt;
    return static_cast<size_t>(item->data(COLUMN_NAME, cheat_role).toULongLong());
}

void CheatsDialog::set_app(const std::string &title_id, const QString &app_title) {
    m_focus_title_id = string_utils::toupper(title_id);
    m_focus_title = app_title;
    reload();

    const std::string target = m_focus_title_id.empty() ? running_title_id() : m_focus_title_id;
    if (Game *game = find_game(target)) {
        game->item->setExpanded(true);
        select_game(target);
    }
}

void CheatsDialog::on_reload() {
    // Re-reading lets the file be edited without a restart, at the cost of dropping toggles.
    const auto running = running_title_id();
    if (!running.empty()) {
        cheat::reload(emuenv.cheat, emuenv.cheat_path, running, emuenv.mem,
            [this](uint32_t address, size_t size) { emuenv.kernel.invalidate_jit_cache(address, size); });
    }

    reload();
}

void CheatsDialog::reload() {
    std::string selected_title_id;
    std::optional<size_t> selected_index;
    if (const Game *game = selected_game()) {
        selected_title_id = game->info.title_id;
        selected_index = selected_cheat();
    }

    std::set<std::string> expanded;
    for (const auto &game : m_games) {
        if (game.item && game.item->isExpanded())
            expanded.insert(game.info.title_id);
    }

    std::map<std::string, QString> app_titles;
    for (const auto &app : app::get_apps(emuenv))
        app_titles.emplace(app.title_id, QString::fromStdString(app.title));

    const std::string running = running_title_id();

    m_games.clear();
    for (auto &info : cheat::list_cheat_games(emuenv.cheat_path))
        m_games.push_back(Game{ .info = std::move(info) });

    // A game without any cheat file is still listed when it runs or the app list asked for it, the search adds to it.
    for (const auto &title_id : { running, m_focus_title_id }) {
        if (!title_id.empty() && !find_game(title_id))
            m_games.push_back(Game{ .info = { .title_id = title_id } });
    }

    for (auto &game : m_games) {
        const auto &title_id = game.info.title_id;
        game.installed = app_titles.contains(title_id);
        if (const auto it = app_titles.find(title_id); it != app_titles.end())
            game.title = it->second;
        else if (title_id == running)
            game.title = QString::fromStdString(emuenv.current_app_title);
        else if (title_id == m_focus_title_id)
            game.title = m_focus_title;
        if (game.title.isEmpty())
            game.title = QString::fromStdString(game.info.name);
    }

    std::ranges::sort(m_games, [&](const Game &a, const Game &b) {
        const bool a_running = a.info.title_id == running;
        const bool b_running = b.info.title_id == running;
        if (a_running != b_running)
            return a_running;
        const QString &a_title = a.title.isEmpty() ? QString::fromStdString(a.info.title_id) : a.title;
        const QString &b_title = b.title.isEmpty() ? QString::fromStdString(b.info.title_id) : b.title;
        return a_title.compare(b_title, Qt::CaseInsensitive) < 0;
    });

    m_populating = true;
    m_tree->clear();
    for (size_t i = 0; i < m_games.size(); ++i) {
        Game &game = m_games[i];
        const QString title_id = QString::fromStdString(game.info.title_id);

        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(COLUMN_NAME, game.title.isEmpty() ? title_id : tr("%1 (%2)").arg(game.title, title_id));
        item->setData(COLUMN_NAME, game_role, static_cast<qulonglong>(i));
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        if (is_live(game)) {
            auto font = item->font(COLUMN_NAME);
            font.setBold(true);
            item->setFont(COLUMN_NAME, font);
            item->setToolTip(COLUMN_NAME, tr("Running, its cheats are turned on and off right away."));
        }
        game.item = item;

        if (is_live(game) || expanded.contains(game.info.title_id)) {
            load_game(game);
            item->setExpanded(true);
        }
    }
    m_populating = false;

    if (!selected_title_id.empty())
        select_game(selected_title_id, selected_index);
    else if (!running.empty())
        select_game(running);

    apply_filter();
    update_search_state();
    on_selection_changed();
}

void CheatsDialog::load_game(Game &game) {
    if (game.loaded)
        return;

    if (is_live(game)) {
        game.file = cheat::snapshot(emuenv.cheat);
    } else if (!game.info.path.empty()) {
        game.file = cheat::parse_cheat_file(game.info.path, game.info.title_id);
        for (auto &cheat : game.file.cheats)
            cheat.enabled = cheat.enabled_on_boot;
    }
    game.loaded = true;

    populate_cheats(game);
}

void CheatsDialog::populate_cheats(Game &game) {
    const bool was_populating = m_populating;
    m_populating = true;

    auto *item = game.item;
    qDeleteAll(item->takeChildren());

    const auto game_index = item->data(COLUMN_NAME, game_role);
    for (size_t i = 0; i < game.file.cheats.size(); ++i) {
        const auto &cheat = game.file.cheats[i];

        auto *child = new QTreeWidgetItem(item);
        child->setText(COLUMN_NAME, QString::fromStdString(cheat.name));
        child->setData(COLUMN_NAME, game_role, game_index);
        child->setData(COLUMN_NAME, cheat_role, static_cast<qulonglong>(i));
        child->setFlags(child->flags() | Qt::ItemIsUserCheckable);
        child->setCheckState(COLUMN_NAME, cheat.enabled ? Qt::Checked : Qt::Unchecked);
        child->setText(COLUMN_CODES, QString::number(cheat.lines.size()));
        child->setText(COLUMN_ON_BOOT, cheat.enabled_on_boot ? tr("Yes") : tr("No"));
        if (cheat.broken) {
            child->setForeground(COLUMN_NAME, palette().brush(QPalette::Disabled, QPalette::Text));
            child->setToolTip(COLUMN_NAME, tr("Vita3K could not interpret these codes, so the cheat is not applied."));
        }
    }

    item->setText(COLUMN_CODES, game.file.cheats.empty() ? QString() : tr("%n cheat(s)", "", static_cast<int>(game.file.cheats.size())));
    if (game.file.cheats.empty())
        item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);

    m_populating = was_populating;
}

void CheatsDialog::select_game(const std::string &title_id, std::optional<size_t> cheat) {
    Game *game = find_game(title_id);
    if (!game || !game->item)
        return;

    QTreeWidgetItem *target = game->item;
    if (cheat && game->loaded && (*cheat < static_cast<size_t>(game->item->childCount()))) {
        game->item->setExpanded(true);
        target = game->item->child(static_cast<int>(*cheat));
    }

    m_tree->setCurrentItem(target);
    m_tree->scrollToItem(target);
}

void CheatsDialog::on_item_expanded(QTreeWidgetItem *item) {
    if (!item || item->parent())
        return;

    const auto index = item->data(COLUMN_NAME, game_role).toULongLong();
    if (index >= m_games.size() || m_games[index].loaded)
        return;

    load_game(m_games[index]);
    apply_filter();
    update_buttons();
    update_status();
}

void CheatsDialog::on_selection_changed() {
    update_buttons();
    update_status();
    update_details();
}

void CheatsDialog::apply_filter() {
    const QString needle = m_filter->text().trimmed();
    const bool installed_only = m_installed_only->isChecked();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *game_item = m_tree->topLevelItem(i);
        const auto index = game_item->data(COLUMN_NAME, game_role).toULongLong();
        if (installed_only && (index < m_games.size()) && !m_games[index].installed && !is_live(m_games[index])) {
            game_item->setHidden(true);
            continue;
        }

        const bool game_matches = needle.isEmpty() || game_item->text(COLUMN_NAME).contains(needle, Qt::CaseInsensitive);

        bool any_cheat_matches = false;
        for (int c = 0; c < game_item->childCount(); ++c) {
            auto *cheat_item = game_item->child(c);
            const bool matches = cheat_item->text(COLUMN_NAME).contains(needle, Qt::CaseInsensitive);
            cheat_item->setHidden(!game_matches && !matches);
            any_cheat_matches |= !needle.isEmpty() && matches;
        }

        game_item->setHidden(!game_matches && !any_cheat_matches);
        if (any_cheat_matches && !game_matches)
            game_item->setExpanded(true);
    }
}

void CheatsDialog::update_buttons() {
    const Game *game = selected_game();
    const bool has_cheats = game && game->loaded && !game->file.cheats.empty();
    m_enable_all_button->setEnabled(has_cheats);
    m_disable_all_button->setEnabled(has_cheats);
    m_save_button->setEnabled(has_cheats);
    // A file that parsed to nothing still opens, that is how the user finds out what is wrong.
    m_open_file_button->setEnabled(game && !game->info.path.empty());
}

void CheatsDialog::update_status() {
    const Game *game = selected_game();
    if (!game) {
        const auto installed = std::ranges::count_if(m_games, [](const Game &game) { return game.installed; });
        if (m_games.empty()) {
            m_status->setText(tr("No cheat file in %1. Put FinalCheat / VitaCheat databases there, named after the title ID of the game (e.g. PCSA00147.psv).")
                    .arg(gui::utils::to_qt_path(emuenv.cheat_path)));
        } else {
            m_status->setText(tr("%n game(s) with cheats, %1 of them installed.", "", static_cast<int>(m_games.size())).arg(installed));
        }
        return;
    }

    if (!game->loaded) {
        m_status->clear();
        return;
    }

    const auto enabled = std::ranges::count_if(game->file.cheats, [](const cheat::Cheat &cheat) { return cheat.enabled; });
    QString status = tr("%n cheat(s) on out of %1", "", static_cast<int>(enabled)).arg(game->file.cheats.size());
    if (!game->file.cheats.empty() && !is_live(*game))
        status += QStringLiteral(" — ") + tr("this game is not running, press Save to have these cheats on when it boots");

    m_status->setText(status);
}

void CheatsDialog::update_details() {
    Game *game = selected_game();
    if (!game) {
        m_details->clear();
        return;
    }

    const auto index = selected_cheat();
    if (!index || !game->loaded || (*index >= game->file.cheats.size())) {
        QString text = QString::fromStdString(game->info.title_id);
        if (!game->title.isEmpty())
            text += QStringLiteral(" ") + game->title;
        text += QStringLiteral("\n");
        if (game->info.path.empty()) {
            text += tr("No cheat file yet. Search for a value below and add it as a cheat, or put %1.psv in %2.")
                        .arg(QString::fromStdString(game->info.title_id), gui::utils::to_qt_path(emuenv.cheat_path));
        } else {
            text += gui::utils::to_qt_path(game->info.path);
            if (game->loaded && !game->file.header.empty())
                text += QStringLiteral("\n") + QString::fromStdString(game->file.header);
        }
        m_details->setPlainText(text);
        return;
    }

    const auto &cheat = game->file.cheats[*index];
    QString text = QStringLiteral("_V%1 %2\n").arg(cheat.enabled_on_boot ? 1 : 0).arg(QString::fromStdString(cheat.name));
    for (const auto &line : cheat.lines)
        text += QString::fromStdString(cheat::format_code_line(line)) + QStringLiteral("\n");

    if (cheat.broken)
        text += QStringLiteral("\n") + tr("Vita3K could not interpret these codes, so the cheat is not applied. See the log for details.");

    m_details->setPlainText(text);
}

void CheatsDialog::set_cheat_enabled(Game &game, size_t index, bool enabled) {
    if (index >= game.file.cheats.size())
        return;

    game.file.cheats[index].enabled = enabled;

    if (!is_live(game))
        return;

    cheat::set_cheat_enabled(emuenv.cheat, index, enabled, emuenv.mem,
        [this](uint32_t address, size_t size) { emuenv.kernel.invalidate_jit_cache(address, size); });
}

void CheatsDialog::set_all_enabled(bool enabled) {
    Game *game = selected_game();
    if (!game || !game->loaded)
        return;

    m_populating = true;
    for (size_t i = 0; i < game->file.cheats.size(); ++i) {
        set_cheat_enabled(*game, i, enabled);
        if (auto *item = game->item->child(static_cast<int>(i)))
            item->setCheckState(COLUMN_NAME, enabled ? Qt::Checked : Qt::Unchecked);
    }
    m_populating = false;

    update_status();
}

void CheatsDialog::on_item_changed(QTreeWidgetItem *item, int column) {
    if (m_populating || !item || !item->parent() || (column != COLUMN_NAME))
        return;

    const auto game_index = item->data(COLUMN_NAME, game_role).toULongLong();
    if (game_index >= m_games.size())
        return;

    set_cheat_enabled(m_games[game_index], static_cast<size_t>(item->data(COLUMN_NAME, cheat_role).toULongLong()),
        item->checkState(COLUMN_NAME) == Qt::Checked);
    update_status();
    update_details();
}

void CheatsDialog::on_master_toggled(bool enabled) {
    if (emuenv.cfg.enable_cheats == enabled)
        return;

    emuenv.cfg.enable_cheats = enabled;
    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);

    cheat::set_enabled(emuenv.cheat, enabled, emuenv.mem,
        [this](uint32_t address, size_t size) { emuenv.kernel.invalidate_jit_cache(address, size); });
}

void CheatsDialog::on_save() {
    Game *game = selected_game();
    if (!game || !game->loaded)
        return;

    const auto path = game->file.path;
    const bool saved = is_live(*game) ? cheat::save(emuenv.cheat) : cheat::save_cheat_file(game->file);

    if (saved) {
        gui::utils::show_message_box(this, QMessageBox::Information, tr("Cheats"),
            tr("The cheats that are on were saved to %1.").arg(gui::utils::to_qt_path(path)),
            { { QStringLiteral("ok"), tr("OK"), QMessageBox::AcceptRole, true } });
    } else {
        gui::utils::show_message_box(this, QMessageBox::Warning, tr("Cheats"),
            tr("Failed to save %1. Check the log for details.").arg(gui::utils::to_qt_path(path)),
            { { QStringLiteral("ok"), tr("OK"), QMessageBox::AcceptRole, true } });
    }

    reload();
}

void CheatsDialog::on_open_file() {
    const Game *game = selected_game();
    if (!game || game->info.path.empty())
        return;

    // `.psv` is not a registered extension, so fall back to the folder when the shell refuses.
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(gui::utils::to_qt_path(game->info.path))))
        gui::utils::open_dir(game->info.path.parent_path());
}

cheat::SearchType CheatsDialog::picked_type() const {
    return static_cast<cheat::SearchType>(m_search_type->currentData().toInt());
}

void CheatsDialog::update_search_state() {
    const std::string running = running_title_id();
    if (running != m_search_title_id) {
        m_search.clear();
        m_search_title_id = running;
        m_current_value->clear();
    }

    const bool can_search = !running.empty();
    if (can_search) {
        const Game *game = find_game(running);
        const QString name = (game && !game->title.isEmpty()) ? game->title : QString::fromStdString(running);
        m_search_hint->setText(tr("Search the memory of %1 for a value, change it in game, then filter the results until only its address is left.").arg(name));
    } else {
        m_search_hint->setText(tr("Start a game to search its memory."));
    }

    m_search_value->setEnabled(can_search);
    m_search_type->setEnabled(can_search);
    m_new_search_button->setEnabled(can_search);
    m_results->setEnabled(can_search);

    if (can_search)
        m_refresh_timer->start();
    else
        m_refresh_timer->stop();

    show_results();
}

void CheatsDialog::show_results() {
    const auto &results = m_search.results();
    const int shown = static_cast<int>(std::min<size_t>(results.size(), max_shown_results));

    m_results->clearContents();
    m_results->setRowCount(shown);
    for (int row = 0; row < shown; ++row) {
        m_results->setItem(row, RESULT_ADDRESS, new QTableWidgetItem(format_address(results[row])));
        m_results->setItem(row, RESULT_VALUE, new QTableWidgetItem());
    }
    refresh_result_values();

    if (!m_search.has_searched()) {
        m_results_label->clear();
    } else {
        QString text = tr("%n result(s)", "", static_cast<int>(std::min<size_t>(results.size(), INT_MAX)));
        if (static_cast<size_t>(shown) < results.size())
            text += QStringLiteral(" — ") + tr("showing the first %1, filter to narrow them down").arg(max_shown_results);
        if (m_search.truncated())
            text += QStringLiteral("\n") + tr("The search stopped after %1 matches, the addresses past them are missing. Search for a value that is rarer.").arg(cheat::MemorySearch::max_results);
        m_results_label->setText(text);
    }

    const bool running = !running_title_id().empty();
    m_filter_results_button->setEnabled(running && m_search.has_searched());
    on_result_selected();
}

void CheatsDialog::refresh_result_values() {
    if (!isVisible() || running_title_id().empty())
        return;

    const auto type = m_search.type();
    const auto &results = m_search.results();
    for (int row = 0; row < m_results->rowCount(); ++row) {
        auto *item = m_results->item(row, RESULT_VALUE);
        if (!item || (static_cast<size_t>(row) >= results.size()))
            continue;

        const auto value = cheat::read_search_value(emuenv.mem, results[row], type);
        const QString text = value ? QString::fromStdString(cheat::format_search_value(*value, type)) : tr("unreadable");
        if (item->text() != text)
            item->setText(text);
    }
}

std::optional<uint32_t> CheatsDialog::selected_address() const {
    const int row = m_results->currentRow();
    const auto &results = m_search.results();
    if ((row < 0) || (static_cast<size_t>(row) >= results.size()) || m_results->selectedItems().isEmpty())
        return std::nullopt;
    return results[row];
}

void CheatsDialog::on_result_selected() {
    const auto address = selected_address();
    const bool can_edit = address && !running_title_id().empty();
    m_current_value->setEnabled(can_edit);
    m_apply_button->setEnabled(can_edit);
    m_add_cheat_button->setEnabled(can_edit);

    if (!can_edit) {
        m_current_value->clear();
        return;
    }

    const auto value = cheat::read_search_value(emuenv.mem, *address, m_search.type());
    m_current_value->setText(value ? QString::fromStdString(cheat::format_search_value(*value, m_search.type())) : QString());
}

void CheatsDialog::on_new_search() {
    if (running_title_id().empty())
        return;

    const auto type = picked_type();
    const auto value = cheat::parse_search_value(m_search_value->text().toStdString(), type);
    if (!value) {
        m_results_label->setText(tr("\"%1\" is not a valid %2 value.").arg(m_search_value->text(), m_search_type->currentText()));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_search.new_search(emuenv.mem, type, *value);
    QApplication::restoreOverrideCursor();

    show_results();
}

void CheatsDialog::on_filter_results() {
    if (running_title_id().empty() || !m_search.has_searched())
        return;

    const auto value = cheat::parse_search_value(m_search_value->text().toStdString(), m_search.type());
    if (!value) {
        m_results_label->setText(tr("\"%1\" is not a valid %2 value.").arg(m_search_value->text(), m_search_type->currentText()));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_search.filter(emuenv.mem, *value);
    QApplication::restoreOverrideCursor();

    show_results();
}

void CheatsDialog::on_apply_value() {
    const auto address = selected_address();
    if (!address)
        return;

    const auto value = cheat::parse_search_value(m_current_value->text().toStdString(), m_search.type());
    if (!value || !cheat::write_search_value(emuenv.mem, *address, m_search.type(), *value)) {
        m_results_label->setText(tr("Could not write \"%1\" to %2.").arg(m_current_value->text(), format_address(*address)));
        return;
    }

    refresh_result_values();
}

void CheatsDialog::on_add_cheat() {
    const auto address = selected_address();
    const std::string running = running_title_id();
    if (!address || running.empty())
        return;

    const auto type = m_search.type();
    auto value = cheat::parse_search_value(m_current_value->text().toStdString(), type);
    if (!value)
        value = cheat::read_search_value(emuenv.mem, *address, type);
    if (!value)
        return;

    bool ok = false;
    const QString suggested = tr("%1 = %2").arg(format_address(*address), QString::fromStdString(cheat::format_search_value(*value, type)));
    const QString name = QInputDialog::getText(this, tr("Add Cheat"),
        tr("The cheat keeps %1 at %2. Name it:").arg(format_address(*address), QString::fromStdString(cheat::format_search_value(*value, type))),
        QLineEdit::Normal, suggested, &ok)
                             .simplified();
    if (!ok || name.isEmpty())
        return;

    cheat::Cheat added;
    added.name = name.toStdString();
    added.enabled = true;
    added.lines.push_back(cheat::make_write_code(*address, type, *value));

    if (!cheat::add_cheat(emuenv.cheat, emuenv.cheat_path, running, added, emuenv.mem,
            [this](uint32_t address, size_t size) { emuenv.kernel.invalidate_jit_cache(address, size); })) {
        gui::utils::show_message_box(this, QMessageBox::Warning, tr("Cheats"),
            tr("Failed to add the cheat to the cheat file of %1. Check the log for details.").arg(QString::fromStdString(running)),
            { { QStringLiteral("ok"), tr("OK"), QMessageBox::AcceptRole, true } });
        return;
    }

    reload();
    if (const Game *game = find_game(running); game && !game->file.cheats.empty())
        select_game(running, game->file.cheats.size() - 1);
}

void CheatsDialog::on_results_context_menu(const QPoint &pos) {
    if (!m_results->itemAt(pos) || !selected_address())
        return;

    QMenu menu(this);
    menu.addAction(tr("Add as Cheat..."), this, &CheatsDialog::on_add_cheat);
    menu.addAction(tr("Copy Address"), this, [this] {
        if (const auto address = selected_address())
            QGuiApplication::clipboard()->setText(format_address(*address));
    });
    menu.exec(m_results->viewport()->mapToGlobal(pos));
}
