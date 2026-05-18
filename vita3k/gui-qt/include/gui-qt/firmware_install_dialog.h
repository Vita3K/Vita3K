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

#include <QDialog>
#include <QObject>
#include <QString>

#include <emuenv/state.h>
#include <packages/functions.h>
#include <util/fs.h>

class FirmwareWorker : public QObject {
    Q_OBJECT
public:
    FirmwareWorker(EmuEnvState &emuenv, const fs::path &pup_path, QObject *parent = nullptr)
        : QObject(parent)
        , m_emuenv(emuenv)
        , m_pup_path(pup_path) {}

    QString fw_version() const { return m_fw_version; }

Q_SIGNALS:
    void progress(float pct);
    void finished(bool success);

public Q_SLOTS:
    void run();

private:
    EmuEnvState &m_emuenv;
    fs::path m_pup_path;
    QString m_fw_version;
};

class FirmwareInstallDialog : public QDialog {
    Q_OBJECT
public:
    explicit FirmwareInstallDialog(EmuEnvState &emuenv, QWidget *parent = nullptr);

Q_SIGNALS:
    void install_complete();

private:
    void run_install(const QString &path);

    EmuEnvState &m_emuenv;
};
