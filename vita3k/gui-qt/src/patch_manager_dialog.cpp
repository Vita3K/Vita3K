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

#include <gui-qt/patch_manager_dialog.h>

#include <app/functions.h>
#include <emuenv/state.h>
#include <util/fs.h>
#include <util/net_utils.h>

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QThread>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int ENTRY_INDEX_ROLE = Qt::UserRole;

QLineEdit *make_read_only_field() {
    auto *field = new QLineEdit;
    field->setReadOnly(true);
    return field;
}

QString entry_label(const PatchEntry &entry) {
    QStringList details;
    if (!entry.app_ver.empty())
        details << QString::fromStdString(entry.app_ver);
    if (entry.app_elf != "eboot.bin")
        details << QString::fromStdString(entry.app_elf);
    const QString name = QString::fromStdString(entry.name);
    return details.isEmpty() ? name : QStringLiteral("%1 (%2)").arg(name, details.join(QStringLiteral(", ")));
}

} // namespace

PatchManagerDialog::PatchManagerDialog(EmuEnvState &emuenv, QWidget *parent)
    : QDialog(parent)
    , m_emuenv(emuenv) {
    setWindowTitle(tr("Patch Manager"));
    resize(1000, 600);

    for (const auto &app : app::get_apps(emuenv))
        m_owned_versions[app.title_id] = app.app_ver;

    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Filter patches"));
    m_filter->setClearButtonEnabled(true);
    m_owned_only = new QCheckBox(tr("Only show owned games"));
    m_owned_only->setChecked(true);

    m_tree = new QTreeWidget;
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);

    auto *filter_row = new QHBoxLayout;
    filter_row->addWidget(m_filter, 1);
    filter_row->addWidget(m_owned_only);

    auto *left = new QVBoxLayout;
    left->addLayout(filter_row);
    left->addWidget(m_tree, 1);

    m_title = make_read_only_field();
    m_serial = make_read_only_field();
    m_game_version = make_read_only_field();
    m_nid = make_read_only_field();
    m_patch_name = make_read_only_field();
    m_patch_version = make_read_only_field();
    m_author = make_read_only_field();
    m_notes = new QPlainTextEdit;
    m_notes->setReadOnly(true);

    auto *form = new QFormLayout;
    form->addRow(tr("Game Title"), m_title);
    form->addRow(tr("Serial"), m_serial);
    form->addRow(tr("Game Version"), m_game_version);
    form->addRow(tr("NID"), m_nid);
    form->addRow(tr("Patch"), m_patch_name);
    form->addRow(tr("Patch Version"), m_patch_version);
    form->addRow(tr("Author"), m_author);
    form->addRow(tr("Notes"), m_notes);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto *info_box = new QGroupBox(tr("Patch Information"));
    info_box->setLayout(form);
    info_box->setMinimumWidth(340);

    auto *content = new QHBoxLayout;
    content->addLayout(left, 3);
    content->addWidget(info_box, 2);

    m_download = new QPushButton(tr("Download latest patches"));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);

    auto *bottom = new QHBoxLayout;
    bottom->addWidget(m_download);
    bottom->addStretch(1);
    bottom->addWidget(buttons);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(content, 1);
    layout->addWidget(new QLabel(tr("Changes take effect the next time a game is started.")));
    layout->addLayout(bottom);

    connect(m_filter, &QLineEdit::textChanged, this, &PatchManagerDialog::apply_filter);
    connect(m_owned_only, &QCheckBox::toggled, this, &PatchManagerDialog::apply_filter);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &PatchManagerDialog::update_info);
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item) {
        const QVariant index = item->data(0, ENTRY_INDEX_ROLE);
        if (!index.isValid())
            return;

        const std::string key = m_entries[index.toInt()].key();
        if (item->checkState(0) == Qt::Checked)
            m_enabled.insert(key);
        else
            m_enabled.erase(key);
    });
    connect(m_download, &QPushButton::clicked, this, &PatchManagerDialog::download_patches);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (save())
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &PatchManagerDialog::save);

    reload_entries();
}

void PatchManagerDialog::reload_entries() {
    m_entries = load_patch_entries(m_emuenv.patch_path);
    m_enabled = load_enabled_patches(m_emuenv.patch_path);
    populate_tree();
}

void PatchManagerDialog::populate_tree() {
    const QSignalBlocker blocker(m_tree);
    m_tree->clear();

    std::vector<int> order(m_entries.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = static_cast<int>(i);
    std::stable_sort(order.begin(), order.end(), [this](int a, int b) {
        return QString::compare(QString::fromStdString(m_entries[a].title), QString::fromStdString(m_entries[b].title), Qt::CaseInsensitive) < 0;
    });

    QTreeWidgetItem *group = nullptr;
    for (const int index : order) {
        const auto &entry = m_entries[index];
        if (entry.nid.empty())
            continue;

        const QString title = QString::fromStdString(entry.title);
        if (!group || group->text(0) != title) {
            group = new QTreeWidgetItem(m_tree, QStringList{ title });
        }

        auto *item = new QTreeWidgetItem(group, QStringList{ entry_label(entry) });
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(0, ENTRY_INDEX_ROLE, index);
        item->setCheckState(0, m_enabled.contains(entry.key()) ? Qt::Checked : Qt::Unchecked);
    }

    apply_filter();
    update_info();
}

void PatchManagerDialog::apply_filter() {
    const QString filter = m_filter->text().trimmed();
    const bool owned_only = m_owned_only->isChecked();

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *group = m_tree->topLevelItem(i);
        int visible_children = 0;

        for (int j = 0; j < group->childCount(); ++j) {
            auto *item = group->child(j);
            const auto &entry = m_entries[item->data(0, ENTRY_INDEX_ROLE).toInt()];

            bool visible = true;
            if (owned_only)
                visible = std::any_of(entry.title_ids.begin(), entry.title_ids.end(), [this, &entry](const std::string &id) {
                    const auto owned = m_owned_versions.find(id);
                    return owned != m_owned_versions.end() && (entry.app_ver.empty() || patch_versions_equal(entry.app_ver, owned->second));
                });
            if (visible && !filter.isEmpty()) {
                QString haystack = QString::fromStdString(entry.title + ' ' + entry.name);
                for (const auto &id : entry.title_ids)
                    haystack += QLatin1Char(' ') + QString::fromStdString(id);
                visible = haystack.contains(filter, Qt::CaseInsensitive);
            }

            item->setHidden(!visible);
            visible_children += visible;
        }
        group->setHidden(visible_children == 0);
    }
}

void PatchManagerDialog::update_info() {
    const auto *item = m_tree->currentItem();
    const QVariant index = item ? item->data(0, ENTRY_INDEX_ROLE) : QVariant();

    if (!index.isValid()) {
        for (auto *field : { m_title, m_serial, m_game_version, m_nid, m_patch_name, m_patch_version, m_author })
            field->clear();
        m_notes->clear();
        return;
    }

    const auto &entry = m_entries[index.toInt()];
    QStringList serials;
    for (const auto &id : entry.title_ids)
        serials << QString::fromStdString(id);

    m_title->setText(QString::fromStdString(entry.title));
    m_serial->setText(serials.join(QStringLiteral(", ")));
    m_game_version->setText(QString::fromStdString(entry.app_ver));
    m_nid->setText(QString::fromStdString(entry.nid));
    m_patch_name->setText(QString::fromStdString(entry.name));
    m_patch_version->setText(QString::fromStdString(entry.patch_ver));
    m_author->setText(QString::fromStdString(entry.author));
    m_notes->setPlainText(QString::fromStdString(entry.note));
}

bool PatchManagerDialog::save() {
    if (save_enabled_patches(m_emuenv.patch_path, m_enabled))
        return true;

    QMessageBox::critical(this, tr("Patch Manager"), tr("Failed to save the patch configuration."));
    return false;
}

void PatchManagerDialog::download_patches() {
    if (m_download_thread)
        return;

    m_download->setEnabled(false);
    m_download->setText(tr("Downloading..."));

    const fs::path patch_dir = m_emuenv.patch_path;
    m_download_thread = QThread::create([self = QPointer<PatchManagerDialog>(this), patch_dir] {
        const std::string xml = net_utils::get_web_response(PATCHLIST_XML_URL);
        PatchEntries entries;
        bool success = false;
        QString message;
        if (xml.empty()) {
            message = QCoreApplication::translate("PatchManagerDialog", "Could not download the patch list. Please try again later.");
        } else if (!parse_patch_entries(xml, entries)) {
            message = QCoreApplication::translate("PatchManagerDialog", "The downloaded patch list is not valid.");
        } else {
            const fs::path path = patchlist_xml_path(patch_dir);
            const fs::path temp_path = fs::path(path).concat(".tmp");
            boost::system::error_code error;
            bool saved = false;
            fs::create_directories(patch_dir, error);
            if (!error && fs_utils::dump_data(temp_path, xml.data(), static_cast<std::streamsize>(xml.size()))) {
                fs::rename(temp_path, path, error);
                saved = !error;
            }
            if (!saved) {
                fs::remove(temp_path, error);
                message = QCoreApplication::translate("PatchManagerDialog", "Failed to save the patch list.");
            } else {
                success = true;
                message = QCoreApplication::translate("PatchManagerDialog", "Downloaded %n patch(es).", nullptr, static_cast<int>(entries.size()));
            }
        }
        const auto finish = [self, success, message] {
            if (self)
                self->on_download_finished(success, message);
        };
        QMetaObject::invokeMethod(qApp, finish, Qt::QueuedConnection);
    });
    connect(m_download_thread, &QThread::finished, m_download_thread, &QObject::deleteLater);
    m_download_thread->start();
}

void PatchManagerDialog::on_download_finished(bool success, const QString &message) {
    m_download->setEnabled(true);
    m_download->setText(tr("Download latest patches"));

    if (success) {
        m_entries = load_patch_entries(m_emuenv.patch_path);
        populate_tree();
        QMessageBox::information(this, tr("Patch Manager"), message);
    } else {
        QMessageBox::warning(this, tr("Patch Manager"), message);
    }
}
