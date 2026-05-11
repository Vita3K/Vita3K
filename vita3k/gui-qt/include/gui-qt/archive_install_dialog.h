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
#include <QString>

#include <archive.h>
#include <emuenv/state.h>
#include <util/fs.h>

#include <vector>

struct ArchiveInstallResult {
    QString archive_name;
    fs::path archive_full_path;
    QString title;
    QString title_id;
    QString category;
    QString content_id;
    bool success = false;
};

class ArchiveWorker : public QObject {
    Q_OBJECT
public:
    ArchiveWorker(EmuEnvState &emuenv,
        const std::vector<fs::path> &archives,
        ReinstallCallback reinstall_cb,
        QObject *parent = nullptr)
        : QObject(parent)
        , m_emuenv(emuenv)
        , m_archives(archives)
        , m_reinstall_cb(std::move(reinstall_cb)) {}

    QList<ArchiveInstallResult> results() const { return m_results; }

Q_SIGNALS:
    void progress(float pct);
    void finished(bool success);
    void global_progress(int done, int total);
    void content_progress(int done, int total);
    void file_progress(float pct);
    void current_title_changed(const QString &title);

public Q_SLOTS:
    void run();

private:
    EmuEnvState &m_emuenv;
    std::vector<fs::path> m_archives;
    ReinstallCallback m_reinstall_cb;
    QList<ArchiveInstallResult> m_results;
};

class ArchiveInstallDialog : public QDialog {
    Q_OBJECT
public:
    explicit ArchiveInstallDialog(EmuEnvState &emuenv, QWidget *parent = nullptr);
    ArchiveInstallDialog(EmuEnvState &emuenv, const std::vector<fs::path> &archives, QWidget *parent = nullptr);

Q_SIGNALS:
    void install_complete(const QList<ArchiveInstallResult> &results);

private:
    std::vector<fs::path> pick_archives();

    void run_install(const std::vector<fs::path> &archives);

    EmuEnvState &m_emuenv;
};
