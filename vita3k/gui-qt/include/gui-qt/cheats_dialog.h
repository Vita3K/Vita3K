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

#include <cheat/cheat.h>
#include <cheat/search.h>

#include <QDialog>

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct EmuEnvState;
class GuiSettings;

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

// Every game of the cheats folder, plus a memory search for the running one.
// Cheats of the running game are toggled live, those of any other game only in memory until saved.
class CheatsDialog : public QDialog {
    Q_OBJECT

public:
    CheatsDialog(EmuEnvState &emuenv, std::shared_ptr<GuiSettings> gui_settings, QWidget *parent = nullptr);

    // Selects the game, listing it even when it has no cheat file yet. An empty id picks the running game.
    void set_app(const std::string &title_id, const QString &app_title);
    void reload();

private Q_SLOTS:
    void on_item_changed(QTreeWidgetItem *item, int column);
    void on_item_expanded(QTreeWidgetItem *item);
    void on_selection_changed();
    void on_reload();
    void on_master_toggled(bool enabled);
    void on_save();
    void on_open_file();
    void on_new_search();
    void on_filter_results();
    void on_result_selected();
    void on_apply_value();
    void on_add_cheat();
    void on_results_context_menu(const QPoint &pos);
    void refresh_result_values();

private:
    struct Game {
        cheat::CheatGame info;
        QString title;
        bool installed = false;
        bool loaded = false;
        cheat::CheatFile file;
        QTreeWidgetItem *item = nullptr;
    };

    bool is_live(const Game &game) const;
    std::string running_title_id() const;
    Game *selected_game();
    std::optional<size_t> selected_cheat() const;
    Game *find_game(const std::string &title_id);
    void load_game(Game &game);
    void populate_cheats(Game &game);
    void select_game(const std::string &title_id, std::optional<size_t> cheat = std::nullopt);
    void set_cheat_enabled(Game &game, size_t index, bool enabled);
    void set_all_enabled(bool enabled);
    void apply_filter();
    void update_buttons();
    void update_status();
    void update_details();
    void update_search_state();
    void show_results();
    cheat::SearchType picked_type() const;
    std::optional<uint32_t> selected_address() const;

    EmuEnvState &emuenv;
    std::shared_ptr<GuiSettings> m_gui_settings;

    std::vector<Game> m_games;
    // Game the app list asked for, kept across reloads.
    std::string m_focus_title_id;
    QString m_focus_title;
    // Set while the tree is being filled, so that filling it does not toggle any cheat.
    bool m_populating = false;

    cheat::MemorySearch m_search;
    std::string m_search_title_id;

    QCheckBox *m_master = nullptr;
    QCheckBox *m_installed_only = nullptr;
    QLineEdit *m_filter = nullptr;
    QTreeWidget *m_tree = nullptr;
    QPlainTextEdit *m_details = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_enable_all_button = nullptr;
    QPushButton *m_disable_all_button = nullptr;
    QPushButton *m_save_button = nullptr;
    QPushButton *m_open_file_button = nullptr;

    QGroupBox *m_search_group = nullptr;
    QLabel *m_search_hint = nullptr;
    QLineEdit *m_search_value = nullptr;
    QComboBox *m_search_type = nullptr;
    QPushButton *m_new_search_button = nullptr;
    QPushButton *m_filter_results_button = nullptr;
    QTableWidget *m_results = nullptr;
    QLabel *m_results_label = nullptr;
    QLineEdit *m_current_value = nullptr;
    QPushButton *m_apply_button = nullptr;
    QPushButton *m_add_cheat_button = nullptr;
    QTimer *m_refresh_timer = nullptr;
};
