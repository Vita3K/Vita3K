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

#include <gui-qt/user_management_dialog.h>

#include <app/functions.h>
#include <config/functions.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <io/state.h>

#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>

UserManagementDialog::UserManagementDialog(EmuEnvState &emuenv, QWidget *parent)
    : QDialog(parent)
    , emuenv(emuenv) {
    setWindowTitle(tr("User Management"));
    setMinimumSize(360, 300);

    auto *layout = new QVBoxLayout(this);

    m_list = new QListWidget(this);
    layout->addWidget(m_list);

    auto *btn_layout = new QHBoxLayout;

    auto *create_btn = new QPushButton(tr("Create User"), this);
    m_select_btn = new QPushButton(tr("Select User"), this);
    m_delete_btn = new QPushButton(tr("Delete User"), this);

    btn_layout->addWidget(create_btn);
    btn_layout->addWidget(m_select_btn);
    btn_layout->addWidget(m_delete_btn);
    layout->addLayout(btn_layout);

    connect(create_btn, &QPushButton::clicked, this, &UserManagementDialog::on_create_user);
    connect(m_select_btn, &QPushButton::clicked, this, &UserManagementDialog::on_select_user);
    connect(m_delete_btn, &QPushButton::clicked, this, &UserManagementDialog::on_delete_user);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &UserManagementDialog::on_select_user);

    refresh_list();
}

void UserManagementDialog::refresh_list() {
    m_list->clear();

    const auto &users = emuenv.app.user_list.users;
    for (const auto &[id, user] : users) {
        const bool is_active = (id == emuenv.io.user_id);
        const QString label = is_active
            ? QStringLiteral("%1 (%2) [active]").arg(QString::fromStdString(user.name), QString::fromStdString(id))
            : QStringLiteral("%1 (%2)").arg(QString::fromStdString(user.name), QString::fromStdString(id));

        auto *item = new QListWidgetItem(label, m_list);
        item->setData(Qt::UserRole, QString::fromStdString(id));
        if (is_active) {
            auto font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
    }

    const bool has_users = m_list->count() > 0;
    m_select_btn->setEnabled(has_users);
    m_delete_btn->setEnabled(has_users);

    if (has_users)
        m_list->setCurrentRow(0);
}

void UserManagementDialog::on_create_user() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Create User"),
        tr("Enter user name:"), QLineEdit::Normal, QStringLiteral("Vita3K"), &ok);

    if (!ok || name.trimmed().isEmpty())
        return;

    const std::string new_id = app::create_user(emuenv, name.trimmed().toStdString());

    if (emuenv.io.user_id.empty()) {
        app::activate_user(emuenv, new_id);
        emuenv.cfg.user_id = new_id;
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }

    refresh_list();
    Q_EMIT user_changed();
}

void UserManagementDialog::on_delete_user() {
    auto *item = m_list->currentItem();
    if (!item)
        return;

    const std::string user_id = item->data(Qt::UserRole).toString().toStdString();
    const auto &users = emuenv.app.user_list.users;
    const auto it = users.find(user_id);
    if (it == users.end())
        return;

    const int result = QMessageBox::question(this, tr("Delete User"),
        tr("Are you sure you want to delete user '%1'?\n\nAll user data will be lost!")
            .arg(QString::fromStdString(it->second.name)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    app::delete_user(emuenv, user_id);

    if (emuenv.io.user_id.empty() && !emuenv.app.user_list.users.empty()) {
        const auto &first = emuenv.app.user_list.users.begin();
        app::activate_user(emuenv, first->first);
        emuenv.cfg.user_id = first->first;
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }

    refresh_list();
    Q_EMIT user_changed();
}

void UserManagementDialog::on_select_user() {
    auto *item = m_list->currentItem();
    if (!item)
        return;

    const std::string user_id = item->data(Qt::UserRole).toString().toStdString();

    if (!app::activate_user(emuenv, user_id))
        return;

    emuenv.cfg.user_id = user_id;
    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);

    refresh_list();
    Q_EMIT user_changed();
}
