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

#include <gui-qt/license_install_dialog.h>
#include <gui-qt/qt_utils.h>

#include <emuenv/state.h>
#include <packages/license.h>
#include <util/fs.h>

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

LicenseInstallDialog::LicenseInstallDialog(EmuEnvState &emuenv, QWidget *parent)
    : QDialog(parent)
    , m_emuenv(emuenv) {
    setWindowTitle(tr("Install License"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("How would you like to install the license?"), this));

    auto *btn_file = new QPushButton(tr("Select .bin / .rif file…"), this);
    auto *btn_zrif = new QPushButton(tr("Enter zRIF key manually…"), this);
    auto *btn_cancel = new QPushButton(tr("Cancel"), this);
    layout->addWidget(btn_file);
    layout->addWidget(btn_zrif);
    layout->addWidget(btn_cancel);

    connect(btn_file, &QPushButton::clicked, this, &LicenseInstallDialog::install_from_file);
    connect(btn_zrif, &QPushButton::clicked, this, &LicenseInstallDialog::install_from_zrif);
    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
}

void LicenseInstallDialog::install_from_file() {
    const QString path_qt = QFileDialog::getOpenFileName(
        this,
        tr("Select License File"),
        QString(),
        tr("PlayStation Vita software license file (*.bin *.rif)"));

    if (path_qt.isEmpty())
        return;

    const bool ok = copy_license(m_emuenv, gui::utils::to_fs_path(path_qt));

    if (!ok) {
        QMessageBox::critical(this, tr("Installation Failed"),
            tr("Failed to install the license file.\nThe file may be corrupted."));
        return;
    }

    QMessageBox info(this);
    info.setWindowTitle(tr("License Installed"));
    info.setIcon(QMessageBox::Information);
    info.setText(tr("License installed successfully!\n\nContent ID: %1\nTitle ID:   %2")
            .arg(QString::fromStdString(m_emuenv.license_content_id),
                QString::fromStdString(m_emuenv.license_title_id)));
    info.exec();
    accept();
}

void LicenseInstallDialog::install_from_zrif() {
    bool ok = false;
    const QString zrif = QInputDialog::getText(
        this,
        tr("Enter zRIF Key"),
        tr("Paste your zRIF key:"),
        QLineEdit::Normal,
        QString(),
        &ok);

    if (!ok || zrif.trimmed().isEmpty())
        return;

    const bool installed = create_license(m_emuenv, zrif.trimmed().toStdString());

    if (!installed) {
        QMessageBox::critical(this, tr("Installation Failed"),
            tr("Failed to create license from zRIF key.\nThe key may be invalid."));
        return;
    }

    QMessageBox info(this);
    info.setWindowTitle(tr("License Installed"));
    info.setIcon(QMessageBox::Information);
    info.setText(tr("License installed successfully!\n\nContent ID: %1\nTitle ID:   %2")
            .arg(QString::fromStdString(m_emuenv.license_content_id),
                QString::fromStdString(m_emuenv.license_title_id)));
    info.exec();
    accept();
}
