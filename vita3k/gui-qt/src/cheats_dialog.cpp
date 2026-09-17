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

#include <gui-qt/qt_utils.h>

#include <cheat/functions.h>
#include <config/functions.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <io/state.h>
#include <kernel/state.h>

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

enum CheatColumn {
    COLUMN_NAME,
    COLUMN_CODES,
    COLUMN_ON_BOOT,
    COLUMN_COUNT,
};

} // namespace

CheatsDialog::CheatsDialog(EmuEnvState &emuenv, QWidget *parent)
    : QDialog(parent)
    , emuenv(emuenv) {
    setWindowTitle(tr("Cheats"));
    setMinimumSize(560, 460);

    auto *layout = new QVBoxLayout(this);

    m_header = new QLabel(this);
    m_header->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_header->setWordWrap(true);
    layout->addWidget(m_header);

    m_master = new QCheckBox(tr("Enable cheats"), this);
    m_master->setToolTip(tr("Master switch. When it is off, no cheat is applied to any game."));
    m_master->setChecked(emuenv.cfg.enable_cheats);
    layout->addWidget(m_master);

    m_enable_all_button = new QPushButton(tr("Enable All"), this);
    m_disable_all_button = new QPushButton(tr("Disable All"), this);

    auto *filter_layout = new QHBoxLayout;
    filter_layout->addWidget(new QLabel(tr("Search:"), this));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Filter cheats by name"));
    m_filter->setClearButtonEnabled(true);
    filter_layout->addWidget(m_filter);
    filter_layout->addWidget(m_enable_all_button);
    filter_layout->addWidget(m_disable_all_button);
    layout->addLayout(filter_layout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(COLUMN_COUNT);
    m_table->setHorizontalHeaderLabels({ tr("Cheat"), tr("Codes"), tr("On boot") });
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(COLUMN_NAME, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COLUMN_CODES, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COLUMN_ON_BOOT, QHeaderView::ResizeToContents);

    m_details = new QPlainTextEdit(this);
    m_details->setReadOnly(true);
    m_details->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_details->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_details->setPlaceholderText(tr("Select a cheat to see its codes"));

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_table);
    splitter->addWidget(m_details);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    m_status = new QLabel(this);
    layout->addWidget(m_status);

    auto *button_layout = new QHBoxLayout;
    m_reload_button = new QPushButton(tr("Reload"), this);
    m_save_button = new QPushButton(tr("Save"), this);
    m_save_button->setToolTip(tr("Write the cheats that are on back to the cheat file, so that they are on again next time the game boots."));
    m_open_file_button = new QPushButton(tr("Open Cheat File"), this);
    m_open_file_button->setToolTip(tr("Open the cheat file of this game in the default editor."));
    auto *open_folder_button = new QPushButton(tr("Open Cheats Folder"), this);

    button_layout->addWidget(m_reload_button);
    button_layout->addWidget(m_save_button);
    button_layout->addWidget(m_open_file_button);
    button_layout->addWidget(open_folder_button);
    button_layout->addStretch();
    layout->addLayout(button_layout);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(m_master, &QCheckBox::toggled, this, &CheatsDialog::on_master_toggled);
    connect(m_filter, &QLineEdit::textChanged, this, &CheatsDialog::on_filter_changed);
    connect(m_table, &QTableWidget::itemChanged, this, &CheatsDialog::on_item_changed);
    connect(m_enable_all_button, &QPushButton::clicked, this, [this] { set_all_enabled(true); });
    connect(m_disable_all_button, &QPushButton::clicked, this, [this] { set_all_enabled(false); });
    connect(m_reload_button, &QPushButton::clicked, this, &CheatsDialog::on_reload);
    connect(m_save_button, &QPushButton::clicked, this, &CheatsDialog::on_save);
    connect(m_open_file_button, &QPushButton::clicked, this, &CheatsDialog::on_open_file);
    connect(open_folder_button, &QPushButton::clicked, this, &CheatsDialog::on_open_folder);
    connect(m_table, &QTableWidget::currentCellChanged, this,
        [this](int row, int, int, int) { on_current_row_changed(row); });

    reload();
}

void CheatsDialog::on_reload() {
    // Re-reading lets the file be edited without a restart, at the cost of dropping toggles.
    if (is_live()) {
        cheat::reload(emuenv.cheat, emuenv.cheat_path, m_title_id, emuenv.mem,
            [this](uint32_t address, size_t size) { emuenv.kernel.invalidate_jit_cache(address, size); });
    }

    reload();
}

void CheatsDialog::set_app(const std::string &title_id, const QString &app_title) {
    m_title_id = title_id;
    m_app_title = app_title;
    reload();
}

bool CheatsDialog::is_live() const {
    return !m_title_id.empty() && (m_title_id == emuenv.io.title_id);
}

void CheatsDialog::reload() {
    if (m_title_id.empty()) {
        m_title_id = emuenv.io.title_id;
        if (m_app_title.isEmpty())
            m_app_title = QString::fromStdString(emuenv.current_app_title);
    }

    if (is_live()) {
        m_file = cheat::snapshot(emuenv.cheat);
    } else {
        m_file = {};
        const auto path = cheat::find_cheat_file(emuenv.cheat_path, m_title_id);
        if (!path.empty()) {
            m_file = cheat::parse_cheat_file(path, m_title_id);
            for (auto &cheat : m_file.cheats)
                cheat.enabled = cheat.enabled_on_boot;
        }
    }

    populate();
}

void CheatsDialog::populate() {
    m_populating = true;

    m_table->clearContents();
    m_table->setRowCount(static_cast<int>(m_file.cheats.size()));

    for (size_t i = 0; i < m_file.cheats.size(); i++) {
        const auto &cheat = m_file.cheats[i];
        const int row = static_cast<int>(i);

        auto *name = new QTableWidgetItem(QString::fromStdString(cheat.name));
        name->setFlags((name->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        name->setCheckState(cheat.enabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(row, COLUMN_NAME, name);

        auto *codes = new QTableWidgetItem(QString::number(cheat.lines.size()));
        codes->setFlags(codes->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, COLUMN_CODES, codes);

        auto *on_boot = new QTableWidgetItem(cheat.enabled_on_boot ? tr("Yes") : tr("No"));
        on_boot->setFlags(on_boot->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, COLUMN_ON_BOOT, on_boot);
    }

    m_populating = false;

    const QString app_name = m_app_title.isEmpty() ? QString::fromStdString(m_title_id) : m_app_title;
    if (m_file.cheats.empty()) {
        const auto cheats_dir = gui::utils::to_qt_path(emuenv.cheat_path);
        m_header->setText(m_title_id.empty()
                ? tr("No game selected. Start a game or pick one in the app list to manage its cheats.")
                : tr("No cheat file for %1 (%2).\nPut a FinalCheat / VitaCheat database named %2.psv in %3.")
                      .arg(app_name, QString::fromStdString(m_title_id), cheats_dir));
    } else {
        const QString description = m_file.header.empty()
            ? tr("%1 (%2)").arg(app_name, QString::fromStdString(m_title_id))
            : QString::fromStdString(m_file.header);
        m_header->setText(tr("%1\n%2").arg(description, gui::utils::to_qt_path(m_file.path)));
    }

    const bool has_cheats = !m_file.cheats.empty();
    m_table->setEnabled(has_cheats);
    m_filter->setEnabled(has_cheats);
    m_enable_all_button->setEnabled(has_cheats);
    m_disable_all_button->setEnabled(has_cheats);
    m_save_button->setEnabled(has_cheats);
    // A file that parsed to nothing still opens, that is how the user finds out what is wrong.
    m_open_file_button->setEnabled(!m_file.path.empty());

    apply_filter();
    update_status();
    update_details();
}

void CheatsDialog::apply_filter() {
    const QString needle = m_filter->text().trimmed();
    for (int row = 0; row < m_table->rowCount(); row++) {
        const auto *item = m_table->item(row, COLUMN_NAME);
        const bool visible = needle.isEmpty() || (item && item->text().contains(needle, Qt::CaseInsensitive));
        m_table->setRowHidden(row, !visible);
    }
}

void CheatsDialog::update_details() {
    const int row = m_table->currentRow();
    if ((row < 0) || (static_cast<size_t>(row) >= m_file.cheats.size())) {
        m_details->clear();
        return;
    }

    const auto &cheat = m_file.cheats[row];

    QString text = QStringLiteral("_V%1 %2\n").arg(cheat.enabled_on_boot ? 1 : 0).arg(QString::fromStdString(cheat.name));
    for (const auto &line : cheat.lines) {
        text += QStringLiteral("$%1 %2 %3\n")
                    .arg(line.control, 4, 16, QLatin1Char('0'))
                    .arg(line.first, 8, 16, QLatin1Char('0'))
                    .arg(line.second, 8, 16, QLatin1Char('0'))
                    .toUpper();
    }

    if (cheat.broken)
        text += QStringLiteral("\n") + tr("Vita3K could not interpret these codes, so the cheat is not applied. See the log for details.");

    m_details->setPlainText(text);
}

void CheatsDialog::on_current_row_changed(int) {
    update_details();
}

void CheatsDialog::update_status() {
    const auto enabled = std::count_if(m_file.cheats.begin(), m_file.cheats.end(),
        [](const cheat::Cheat &cheat) { return cheat.enabled; });

    QString status = tr("%n cheat(s) on out of %1", "", static_cast<int>(enabled))
                         .arg(m_file.cheats.size());
    if (!m_file.cheats.empty() && !is_live())
        status += QStringLiteral(" — ") + tr("this game is not running, the cheats will be applied when it boots");

    m_status->setText(status);
}

void CheatsDialog::set_cheat_enabled(size_t index, bool enabled) {
    if (index >= m_file.cheats.size())
        return;

    m_file.cheats[index].enabled = enabled;

    if (!is_live())
        return;

    cheat::set_cheat_enabled(emuenv.cheat, index, enabled, emuenv.mem,
        [this](uint32_t address, size_t size) { emuenv.kernel.invalidate_jit_cache(address, size); });
}

void CheatsDialog::set_all_enabled(bool enabled) {
    m_populating = true;
    for (size_t i = 0; i < m_file.cheats.size(); i++) {
        set_cheat_enabled(i, enabled);
        if (auto *item = m_table->item(static_cast<int>(i), COLUMN_NAME))
            item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
    m_populating = false;

    update_status();
}

void CheatsDialog::on_item_changed(QTableWidgetItem *item) {
    if (m_populating || !item || (item->column() != COLUMN_NAME))
        return;

    set_cheat_enabled(static_cast<size_t>(item->row()), item->checkState() == Qt::Checked);
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

void CheatsDialog::on_filter_changed(const QString &) {
    apply_filter();
}

void CheatsDialog::on_save() {
    const bool saved = is_live() ? cheat::save(emuenv.cheat) : cheat::save_cheat_file(m_file);

    if (saved) {
        gui::utils::show_message_box(this, QMessageBox::Information, tr("Cheats"),
            tr("The cheats that are on were saved to %1.").arg(gui::utils::to_qt_path(m_file.path)),
            { { QStringLiteral("ok"), tr("OK"), QMessageBox::AcceptRole, true } });
    } else {
        gui::utils::show_message_box(this, QMessageBox::Warning, tr("Cheats"),
            tr("Failed to save %1. Check the log for details.").arg(gui::utils::to_qt_path(m_file.path)),
            { { QStringLiteral("ok"), tr("OK"), QMessageBox::AcceptRole, true } });
    }

    reload();
}

void CheatsDialog::on_open_file() {
    if (m_file.path.empty())
        return;

    // `.psv` is not a registered extension, so fall back to the folder when the shell refuses.
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(gui::utils::to_qt_path(m_file.path))))
        gui::utils::open_dir(m_file.path.parent_path());
}

void CheatsDialog::on_open_folder() {
    gui::utils::open_dir(emuenv.cheat_path);
}
