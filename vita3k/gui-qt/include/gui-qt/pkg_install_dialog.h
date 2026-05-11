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
#include <packages/pkg.h>
#include <util/fs.h>

#include <optional>
#include <string>

class PkgWorker : public QObject {
    Q_OBJECT
public:
    PkgWorker(EmuEnvState &emuenv,
        const fs::path &pkg_path,
        const std::string &zrif,
        QObject *parent = nullptr)
        : QObject(parent)
        , m_emuenv(emuenv)
        , m_pkg_path(pkg_path)
        , m_zrif(zrif) {}

Q_SIGNALS:
    void progress(float pct);
    void finished(bool success);

public Q_SLOTS:
    void run();

private:
    EmuEnvState &m_emuenv;
    fs::path m_pkg_path;
    std::string m_zrif;
};

class PkgInstallDialog : public QDialog {
    Q_OBJECT
public:
    explicit PkgInstallDialog(EmuEnvState &emuenv, QWidget *parent = nullptr);
    PkgInstallDialog(EmuEnvState &emuenv, const fs::path &pkg_path, QWidget *parent = nullptr);
    PkgInstallDialog(EmuEnvState &emuenv, const fs::path &pkg_path, std::optional<fs::path> license_path, QWidget *parent = nullptr);

Q_SIGNALS:
    void install_complete(const QString &title_id);

private:
    QString acquire_zrif(const fs::path &pkg_path, const std::optional<fs::path> &license_path = std::nullopt);

    void run_install(const fs::path &pkg_path, const QString &zrif);

    EmuEnvState &m_emuenv;
    fs::path m_license_path;
};
