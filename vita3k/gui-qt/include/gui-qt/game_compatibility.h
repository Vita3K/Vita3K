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

#include <compat/functions.h>

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <qtypes.h>

#include <filesystem>

class GameCompatibility : public QObject {
    Q_OBJECT
public:
    explicit GameCompatibility(CompatState &state, std::filesystem::path cache_path, QObject *parent = nullptr);

Q_SIGNALS:
    void db_loaded(int app_count);
    void db_updated(int old_count, int new_count, QString old_date, QString new_date);
    void download_started();
    void download_progress(qint64 received, qint64 total);
    void error(QString message);

public Q_SLOTS:
    void request_update();

private Q_SLOTS:
    void on_ver_reply_finished();
    void on_db_reply_finished();

private:
    CompatState &m_state;
    std::filesystem::path m_cache_path;
    QNetworkAccessManager m_network;
    QString m_pending_ver;
    int m_old_count{};
};