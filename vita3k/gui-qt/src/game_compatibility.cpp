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

#include <gui-qt/game_compatibility.h>

#include <compat/functions.h>
#include <compat/state.h>
#include <util/log.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <qtypes.h>

#include <filesystem>
#include <span>

static constexpr const char *k_version_url = "https://api.github.com/repos/Vita3K/compatibility/releases/latest";
static constexpr const char *k_db_url = "https://github.com/Vita3K/compatibility/releases/download/compat_db/app_compat_db.xml.zip";

GameCompatibility::GameCompatibility(CompatState &state, std::filesystem::path cache_path, QObject *parent)
    : QObject(parent)
    , m_state(state)
    , m_cache_path(std::move(cache_path)) {
    if (compat::load_from_disk(m_state, m_cache_path))
        Q_EMIT db_loaded(static_cast<int>(m_state.app_compat_db.size()));

    request_update();
}

void GameCompatibility::request_update() {
    Q_EMIT download_started();
    auto *reply = m_network.get(QNetworkRequest(QUrl(k_version_url)));
    connect(reply, &QNetworkReply::finished, this, &GameCompatibility::on_ver_reply_finished);
}

void GameCompatibility::on_ver_reply_finished() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        Q_EMIT error(reply->errorString());
        return;
    }

    const auto update_info = compat::parse_ver_resp(m_state, reply->readAll().toStdString());

    if (!update_info) {
        Q_EMIT error(tr("Failed to parse version response."));
        return;
    }

    if (!update_info->needs_update) {
        LOG_INFO("DB is up to date ({})", m_state.db_updated_at);
        return;
    }

    m_pending_ver = QString::fromStdString(update_info->latest_ver);
    m_old_count = static_cast<int>(m_state.app_compat_db.size());

    auto *db_reply = m_network.get(QNetworkRequest(QUrl(k_db_url)));
    connect(db_reply, &QNetworkReply::downloadProgress, this, &GameCompatibility::download_progress);
    connect(db_reply, &QNetworkReply::finished, this, &GameCompatibility::on_db_reply_finished);
}

void GameCompatibility::on_db_reply_finished() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        Q_EMIT error(reply->errorString());
        return;
    }

    const QByteArray raw = reply->readAll();
    const QString old_date = QString::fromStdString(m_state.db_updated_at);

    const bool ok = compat::install_db(
        m_state, m_cache_path,
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t *>(raw.constData()), raw.size()),
        m_pending_ver.toStdString());

    if (!ok) {
        Q_EMIT error(tr("Failed to install compatibility database"));
        return;
    }

    Q_EMIT db_updated(m_old_count,
        static_cast<int>(m_state.app_compat_db.size()),
        old_date,
        m_pending_ver);
}