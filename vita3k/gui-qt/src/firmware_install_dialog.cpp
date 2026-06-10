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

#include <gui-qt/firmware_install_dialog.h>
#include <gui-qt/qt_utils.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

void FirmwareWorker::run() {
    const std::string version = install_pup(
        m_emuenv.vita_fs_path,
        m_pup_path,
        [this](uint32_t pct) {
            Q_EMIT progress(static_cast<float>(pct));
        });

    m_fw_version = QString::fromStdString(version);
    Q_EMIT finished(!version.empty());
}

FirmwareInstallDialog::FirmwareInstallDialog(EmuEnvState &emuenv, QWidget *parent)
    : QDialog(parent)
    , m_emuenv(emuenv) {
    setWindowTitle(tr("Install Firmware"));
    setModal(true);

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select Firmware Package"),
        QString(),
        tr("PlayStation Vita Firmware Package (*.PUP *.pup)"));

    if (path.isEmpty()) {
        QMetaObject::invokeMethod(this, &QDialog::reject, Qt::QueuedConnection);
        return;
    }

    run_install(path);
}

FirmwareInstallDialog::FirmwareInstallDialog(EmuEnvState &emuenv, const QString &path, QWidget *parent)
    : QDialog(parent)
    , m_emuenv(emuenv) {
    setWindowTitle(tr("Install Firmware"));
    setModal(true);

    if (path.isEmpty()) {
        QMetaObject::invokeMethod(this, &QDialog::reject, Qt::QueuedConnection);
        return;
    }

    run_install(path);
}

void FirmwareInstallDialog::run_install(const QString &path) {
    auto *main_layout = new QVBoxLayout(this);

    auto *title_label = new QLabel(tr("Installing firmware, please wait…"), this);
    title_label->setAlignment(Qt::AlignCenter);
    main_layout->addWidget(title_label);

    auto *bar = new QProgressBar(this);
    bar->setRange(0, 100);
    bar->setValue(0);
    main_layout->addWidget(bar);

    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

    const fs::path pup_path = gui::utils::to_fs_path(path);
    auto *thread = new QThread(this);
    auto *worker = new FirmwareWorker(m_emuenv, pup_path);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &FirmwareWorker::run);

    connect(worker, &FirmwareWorker::progress, this, [bar](float pct) {
        const int v = static_cast<int>(pct);
        bar->setValue(v);
    });

    connect(worker, &FirmwareWorker::finished, this, [this, worker, thread, path](bool success) {
        thread->quit();
        thread->wait();

        const QString ver = worker->fw_version();
        worker->deleteLater();

        setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);
        show();

        if (!success) {
            QMessageBox::critical(this, tr("Installation Failed"),
                tr("Failed to install firmware.\nCheck the log for details."));
            reject();
            return;
        }

        QLayoutItem *item;
        while ((item = layout()->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        auto *msg = new QLabel(
            tr("Firmware installed successfully!%1")
                .arg(ver.isEmpty() ? QString() : tr("\n\nVersion: %1").arg(ver)),
            this);
        msg->setAlignment(Qt::AlignCenter);
        layout()->addWidget(msg);

        auto *del_check = new QCheckBox(tr("Delete firmware file after install?"), this);
        layout()->addWidget(del_check);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
        layout()->addWidget(buttons);

        connect(buttons, &QDialogButtonBox::accepted, this, [this, del_check, path]() {
            // QFile::remove returns false silently when the firmware file is locked, so surface that to the user
            if (del_check->isChecked() && !QFile::remove(path)) {
                QMessageBox::warning(this, tr("Could Not Delete Firmware"),
                    tr("The firmware file could not be deleted. It may still be open in another program:\n\n%1").arg(path));
            }
            Q_EMIT install_complete();
            accept();
        });

        adjustSize();
    });

    thread->start();
}
