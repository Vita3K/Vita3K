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

#include <gui-qt/update_manager.h>

#include <config/version.h>
#include <gui-qt/qt_utils.h>
#include <updater/functions.h>
#include <util/net_utils.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>
#include <filesystem>
#include <vector>

namespace {

QString tr_update(const char *source_text) {
    return QCoreApplication::translate("UpdateManager", source_text);
}

std::filesystem::path install_directory() {
    return std::filesystem::path(QCoreApplication::applicationDirPath().toStdU16String());
}

QDateTime parse_published_at(const std::string &published_at) {
    return QDateTime::fromString(QString::fromStdString(published_at), Qt::ISODate);
}

QString format_published_at(const std::string &published_at) {
    const QDateTime parsed = parse_published_at(published_at);
    if (!parsed.isValid())
        return published_at.empty() ? tr_update("Unknown") : QString::fromStdString(published_at);

    return QLocale().toString(parsed.toLocalTime(), QLocale::ShortFormat);
}

QString format_relative_age(const std::string &published_at) {
    const QDateTime parsed = parse_published_at(published_at);
    if (!parsed.isValid())
        return {};

    const qint64 seconds = parsed.secsTo(QDateTime::currentDateTimeUtc());
    if (seconds < 60)
        return tr_update("moments ago");
    if (seconds < 3600)
        return tr_update("%1 minute(s) ago").arg(seconds / 60);
    if (seconds < 86400)
        return tr_update("%1 hour(s) ago").arg(seconds / 3600);
    if (seconds < 86400 * 30)
        return tr_update("%1 day(s) ago").arg(seconds / 86400);

    return tr_update("%1 month(s) ago").arg(std::max<qint64>(1, seconds / (86400 * 30)));
}

bool parse_build_number(const QString &body, std::uint64_t &build_number) {
    static const QRegularExpression build_regex(QStringLiteral("Vita3K Build:\\s*(\\d+)"));
    const auto match = build_regex.match(body);
    if (!match.hasMatch())
        return false;

    bool ok = false;
    const qulonglong parsed = match.captured(1).toULongLong(&ok);
    if (!ok || parsed == 0)
        return false;

    build_number = parsed;
    return true;
}

QString normalized_notes(const QString &body) {
    QString notes = body;
    notes.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    QStringList cleaned_lines;
    const QStringList lines = notes.split('\n');
    static const QRegularExpression metadata_line(QStringLiteral("^\\s*(Corresponding commit:|Vita3K Build:)"), QRegularExpression::CaseInsensitiveOption);

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (metadata_line.match(trimmed).hasMatch())
            continue;

        cleaned_lines.append(line);
    }

    while (!cleaned_lines.isEmpty() && cleaned_lines.first().trimmed().isEmpty())
        cleaned_lines.removeFirst();
    while (!cleaned_lines.isEmpty() && cleaned_lines.last().trimmed().isEmpty())
        cleaned_lines.removeLast();

    return cleaned_lines.join('\n').trimmed();
}

QString to_html_with_breaks(const QString &text) {
    QString escaped = text.toHtmlEscaped();
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return escaped;
}

void parse_changelog(const QJsonObject &root, updater::UpdateInfo &info) {
    const QJsonValue changelog_value = root.value(QStringLiteral("changelog"));
    if (!changelog_value.isArray())
        return;

    for (const QJsonValue &entry_value : changelog_value.toArray()) {
        if (!entry_value.isObject())
            continue;

        const QJsonObject entry_object = entry_value.toObject();
        updater::ChangelogEntry entry;
        entry.version = entry_object.value(QStringLiteral("version")).toString().toStdString();
        entry.title = entry_object.value(QStringLiteral("title")).toString().toStdString();
        if (!entry.version.empty() || !entry.title.empty())
            info.changelog.push_back(std::move(entry));
    }
}

bool parse_release_json(const std::string &json, updater::UpdateInfo &info, QString &error_message) {
    QJsonParseError parse_error{};
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(json), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        error_message = tr_update("The GitHub release response could not be parsed.");
        return false;
    }

    const QJsonObject root = document.object();
    const QString body = root.value(QStringLiteral("body")).toString();
    const QString published_at = root.value(QStringLiteral("published_at")).toString();
    const QString release_name = root.value(QStringLiteral("name")).toString().trimmed();
    const QString tag_name = root.value(QStringLiteral("tag_name")).toString().trimmed();
    const QString release_url = root.value(QStringLiteral("html_url")).toString().trimmed();

    if (!parse_build_number(body, info.build_number)) {
        error_message = tr_update("The latest release does not advertise a usable Vita3K build number.");
        return false;
    }

    info.version = (release_name.isEmpty() ? tag_name : release_name).toStdString();
    info.release_url = (release_url.isEmpty() ? QString::fromStdString(updater::release_page_url()) : release_url).toStdString();
    info.published_at = published_at.toStdString();
    info.notes = normalized_notes(body).toStdString();
    parse_changelog(root, info);
    return true;
}

QString build_details_text(const updater::UpdateInfo &info) {
    QStringList sections;

    if (!info.changelog.empty()) {
        QStringList changelog_lines;
        for (const auto &entry : info.changelog) {
            const QString version = entry.version.empty() ? tr_update("N/A") : QString::fromStdString(entry.version);
            const QString title = entry.title.empty() ? tr_update("N/A") : QString::fromStdString(entry.title);
            changelog_lines.append(tr_update("- %1: %2").arg(version, title));
        }
        sections.append(tr_update("Recent changes:\n%1").arg(changelog_lines.join('\n')));
    }

    if (!info.notes.empty())
        sections.append(QString::fromStdString(info.notes));

    return sections.join(QStringLiteral("\n\n"));
}

updater::UpdateCheckResult build_check_result() {
    updater::UpdateCheckResult result;
    result.current_display_version = updater::current_display_version();

    const std::string response = net_utils::get_web_response(updater::release_api_url());
    if (response.empty()) {
        result.status = updater::UpdateCheckStatus::Failed;
        result.message = tr_update("Could not retrieve the latest Vita3K release information. Please try again later.").toStdString();
        return result;
    }

    QString parse_message;
    if (!parse_release_json(response, result.info, parse_message)) {
        result.status = updater::UpdateCheckStatus::Failed;
        result.message = parse_message.toStdString();
        return result;
    }

    const auto current_build_number = static_cast<std::uint64_t>(app_number);
    const auto build_delta = static_cast<std::int64_t>(result.info.build_number) - static_cast<std::int64_t>(current_build_number);

    if (!updater::is_official_build()) {
        result.status = updater::UpdateCheckStatus::CustomBuildCanUpdate;
        return result;
    }

    if (build_delta > 0) {
        result.status = updater::UpdateCheckStatus::UpdateAvailable;
        return result;
    }

    if (build_delta < 0) {
        result.status = updater::UpdateCheckStatus::CurrentBuildNewerThanLatest;
        result.message = tr_update("This installation appears newer than the latest official continuous build.").toStdString();
        return result;
    }

    result.status = updater::UpdateCheckStatus::UpToDate;
    result.message = tr_update("You already have the latest official build installed.").toStdString();
    return result;
}

} // namespace

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent) {
    updater::remove_update_backups(install_directory());
}

UpdateManager::~UpdateManager() {
    if (m_worker_thread) {
        m_worker_thread->quit();
        m_worker_thread->wait();
    }
}

void UpdateManager::set_update_available(const bool available) {
    if (m_update_available == available)
        return;

    m_update_available = available;
    Q_EMIT update_available_changed(available);
}

void UpdateManager::set_pending_update(std::optional<updater::UpdateCheckResult> result) {
    m_pending_update = std::move(result);
    set_update_available(m_pending_update.has_value());
}

void UpdateManager::start_worker(const std::function<void()> &task) {
    if (m_worker_thread)
        return;

    m_worker_thread = QThread::create(task);
    connect(m_worker_thread, &QThread::finished, this, [this]() {
        m_worker_thread->deleteLater();
        m_worker_thread = nullptr;
    });
    m_worker_thread->start();
}

void UpdateManager::close_progress_dialog() {
    if (!m_progress_dialog)
        return;

    m_progress_dialog->close();
    m_progress_dialog->deleteLater();
    m_progress_dialog = nullptr;
}

void UpdateManager::check_for_updates(const updater::UpdateCheckMode mode, QWidget *parent) {
    if (m_download_reply) {
        if (mode == updater::UpdateCheckMode::ManualInteractive)
            QMessageBox::information(parent, tr_update("Check for Updates"), tr_update("An update download is already running."));
        return;
    }

    if (m_worker_thread) {
        if (mode == updater::UpdateCheckMode::ManualInteractive)
            QMessageBox::information(parent, tr_update("Check for Updates"), tr_update("An update check is already running."));
        return;
    }

    m_parent_widget = parent;

    if (mode == updater::UpdateCheckMode::ManualInteractive) {
        auto *dialog = new QProgressDialog(tr_update("Checking for updates..."), QString(), 0, 0, parent);
        dialog->setWindowTitle(tr_update("Check for Updates"));
        dialog->setWindowModality(Qt::WindowModal);
        dialog->setMinimumDuration(0);
        dialog->setCancelButton(nullptr);
        dialog->show();
        m_progress_dialog = dialog;
    }

    start_worker([this, mode]() {
        const updater::UpdateCheckResult result = build_check_result();
        QMetaObject::invokeMethod(
            this, [this, mode, result]() { handle_check_result(mode, result); }, Qt::QueuedConnection);
    });
}

void UpdateManager::review_pending_update(QWidget *parent) {
    m_parent_widget = parent;

    if (m_pending_update) {
        show_update_message(*m_pending_update);
        return;
    }

    check_for_updates(updater::UpdateCheckMode::ManualInteractive, parent);
}

void UpdateManager::handle_check_result(const updater::UpdateCheckMode mode, const updater::UpdateCheckResult &result) {
    close_progress_dialog();

    switch (result.status) {
    case updater::UpdateCheckStatus::Failed:
        if (mode == updater::UpdateCheckMode::ManualInteractive)
            QMessageBox::warning(m_parent_widget, tr_update("Check for Updates"), QString::fromStdString(result.message));
        return;
    case updater::UpdateCheckStatus::UpToDate:
    case updater::UpdateCheckStatus::CurrentBuildNewerThanLatest:
        set_pending_update(std::nullopt);
        if (mode == updater::UpdateCheckMode::ManualInteractive)
            QMessageBox::information(m_parent_widget, tr_update("Check for Updates"), QString::fromStdString(result.message));
        return;
    case updater::UpdateCheckStatus::CustomBuildCanUpdate:
        set_pending_update(std::nullopt);
        if (mode == updater::UpdateCheckMode::ManualInteractive)
            show_update_message(result);
        return;
    case updater::UpdateCheckStatus::UpdateAvailable:
        set_pending_update(result);
        if (mode != updater::UpdateCheckMode::StartupBackground)
            show_update_message(result);
        return;
    }
}

void UpdateManager::show_update_message(const updater::UpdateCheckResult &result) {
    const QString published_at = format_published_at(result.info.published_at);
    const QString published_age = format_relative_age(result.info.published_at);
    const QString details_text = build_details_text(result.info);
    const QString latest_version = result.info.version.empty() ? tr_update("Latest build") : QString::fromStdString(result.info.version);
    const QString download_url = result.info.release_url.empty() ? QString::fromStdString(updater::release_page_url()) : QString::fromStdString(result.info.release_url);

    QString title;
    QString summary;
    switch (result.status) {
    case updater::UpdateCheckStatus::CustomBuildCanUpdate:
        title = tr_update("Official Build Available");
        summary = tr_update(
            "You are currently running a non-official or PR build.\n\n"
            "If you update, you will switch to the latest official build and will no longer be on this custom or PR build.\n\n"
            "Latest official build: %1\n"
            "Published: %2%3")
                      .arg(latest_version)
                      .arg(published_at)
                      .arg(published_age.isEmpty() ? QString() : tr_update(" (%1)").arg(published_age));
        break;
    case updater::UpdateCheckStatus::UpdateAvailable:
    default:
        title = tr_update("Update Available");
        summary = tr_update(
            "A newer Vita3K build is available.\n\n"
            "Current version: %1\n"
            "Latest version: %2\n"
            "Published: %3%4")
                      .arg(QString::fromStdString(result.current_display_version))
                      .arg(latest_version)
                      .arg(published_at)
                      .arg(published_age.isEmpty() ? QString() : tr_update(" (%1)").arg(published_age));
        break;
    }

    std::vector<gui::utils::MessageDialogButton> buttons;
    const bool can_install = updater::can_self_update();
    if (can_install)
        buttons.push_back({ QStringLiteral("install"), tr_update("Update Now"), QMessageBox::AcceptRole, true });
    buttons.push_back({ QStringLiteral("open"), tr_update("Open Download Page"), QMessageBox::ActionRole, !can_install });
    buttons.push_back({ QStringLiteral("ok"), tr_update("OK"), QMessageBox::AcceptRole, false });

    const auto dialog_result = gui::utils::show_message_box(
        m_parent_widget,
        QMessageBox::Information,
        title,
        tr_update("%1<br><br>Download page: <a href=\"%2\">Vita3K GitHub Releases</a>")
            .arg(to_html_with_breaks(summary), download_url.toHtmlEscaped()),
        buttons,
        {},
        {},
        false,
        Qt::RichText,
        Qt::TextBrowserInteraction,
        details_text);

    if (dialog_result.clicked_id == QStringLiteral("install"))
        start_update_download();
    else if (dialog_result.clicked_id == QStringLiteral("open"))
        QDesktopServices::openUrl(QUrl(download_url));
}

void UpdateManager::close_download_dialog() {
    if (!m_download_dialog)
        return;

    m_download_dialog->close();
    m_download_dialog->deleteLater();
    m_download_dialog = nullptr;
}

void UpdateManager::start_update_download() {
    if (m_download_reply || m_worker_thread)
        return;

    const std::string asset_url = updater::release_asset_url();
    if (asset_url.empty()) {
        QMessageBox::warning(m_parent_widget, tr_update("Update Vita3K"), tr_update("This platform cannot install Vita3K updates by itself."));
        return;
    }

    auto *dialog = new QProgressDialog(tr_update("Downloading the latest build..."), tr_update("Cancel"), 0, 100, m_parent_widget);
    dialog->setWindowTitle(tr_update("Update Vita3K"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setMinimumDuration(0);
    dialog->setAutoClose(false);
    dialog->setAutoReset(false);
    dialog->setValue(0);
    dialog->show();
    m_download_dialog = dialog;

    QNetworkRequest request{ QUrl(QString::fromStdString(asset_url)) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *reply = m_network.get(request);
    m_download_reply = reply;

    connect(dialog, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateManager::on_download_progress);
    connect(reply, &QNetworkReply::finished, this, &UpdateManager::on_download_finished);
}

void UpdateManager::on_download_progress(const qint64 received, const qint64 total) {
    if (!m_download_dialog)
        return;

    // GitHub does not always announce a length, and a busy indicator beats a bar stuck at zero
    if (total <= 0) {
        m_download_dialog->setRange(0, 0);
        return;
    }

    constexpr qint64 bytes_per_mib = 1024 * 1024;
    m_download_dialog->setRange(0, 100);
    m_download_dialog->setValue(static_cast<int>(received * 100 / total));
    m_download_dialog->setLabelText(tr_update("Downloading the latest build... (%1 / %2 MiB)")
            .arg(received / bytes_per_mib)
            .arg(total / bytes_per_mib));
}

void UpdateManager::on_download_finished() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    reply->deleteLater();
    m_download_reply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        const bool canceled = reply->error() == QNetworkReply::OperationCanceledError;
        const QString error_string = reply->errorString();
        close_download_dialog();
        if (!canceled)
            QMessageBox::warning(m_parent_widget, tr_update("Update Vita3K"), tr_update("The update could not be downloaded: %1").arg(error_string));
        return;
    }

    const QByteArray archive = reply->readAll();
    if (archive.isEmpty()) {
        close_download_dialog();
        QMessageBox::warning(m_parent_widget, tr_update("Update Vita3K"), tr_update("The downloaded update is empty."));
        return;
    }

    if (m_download_dialog) {
        // Swapping the files cannot be interrupted safely, so the cancel button has to go
        m_download_dialog->setCancelButton(nullptr);
        m_download_dialog->setRange(0, 0);
        m_download_dialog->setLabelText(tr_update("Installing the update..."));
    }

    const std::filesystem::path install_dir = install_directory();
    start_worker([this, archive, install_dir]() {
        std::string error_message;
        const bool installed = updater::install_update(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(archive.constData()), static_cast<size_t>(archive.size())),
            install_dir, error_message);

        const QString message = QString::fromStdString(error_message);
        QMetaObject::invokeMethod(
            this, [this, installed, message]() { finish_update(installed, message); }, Qt::QueuedConnection);
    });
}

void UpdateManager::finish_update(const bool installed, const QString &error_message) {
    close_download_dialog();

    if (!installed) {
        QMessageBox::warning(m_parent_widget, tr_update("Update Vita3K"),
            error_message.isEmpty() ? tr_update("The update could not be installed.") : error_message);
        return;
    }

    set_pending_update(std::nullopt);

    const auto dialog_result = gui::utils::show_message_box(
        m_parent_widget,
        QMessageBox::Information,
        tr_update("Update Vita3K"),
        tr_update("The update has been installed. Vita3K must restart to run the new build."),
        {
            { QStringLiteral("restart"), tr_update("Restart Now"), QMessageBox::AcceptRole, true },
            { QStringLiteral("later"), tr_update("Restart Later"), QMessageBox::RejectRole, false },
        });

    if (dialog_result.clicked_id != QStringLiteral("restart"))
        return;

    // Inside an AppImage the application path points into the read-only mount, not at the file that was replaced
    const std::string appimage = updater::appimage_path();
    const QString program = appimage.empty() ? QCoreApplication::applicationFilePath() : QString::fromStdString(appimage);

    if (QProcess::startDetached(program, {}))
        QCoreApplication::quit();
    else
        QMessageBox::warning(m_parent_widget, tr_update("Update Vita3K"), tr_update("Vita3K could not restart itself, please close and start it again."));
}
