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

#include <miniz.h>

#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSaveFile>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

namespace {

constexpr quint64 update_staging_budget = 2ULL * 1024 * 1024 * 1024;

struct ZipOutputContext {
    QFile *file;
    quint64 remaining;
    std::atomic_bool *cancellation;
};

size_t write_zip_output(void *opaque, mz_uint64 offset, const void *data, size_t size) {
    auto *context = static_cast<ZipOutputContext *>(opaque);
    if (context->cancellation->load(std::memory_order_relaxed))
        return 0;
    if (size > context->remaining || !context->file->seek(static_cast<qint64>(offset)))
        return 0;
    const qint64 written = context->file->write(static_cast<const char *>(data), static_cast<qint64>(size));
    if (written != static_cast<qint64>(size))
        return 0;
    context->remaining -= static_cast<quint64>(written);
    return static_cast<size_t>(written);
}

QString tr_update(const char *source_text) {
    return QCoreApplication::translate("UpdateManager", source_text);
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

QString to_html_with_breaks(const QString &text) {
    QString escaped = text.toHtmlEscaped();
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return escaped;
}

QString expected_asset_name() {
#if defined(Q_OS_WIN)
#if defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("windows-arm64-latest.zip");
#else
    return QStringLiteral("windows-latest.zip");
#endif
#elif defined(Q_OS_MACOS)
#if defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("macos-arm64-latest.dmg");
#else
    return QStringLiteral("macos-latest.dmg");
#endif
#elif defined(Q_OS_LINUX)
    if (!qEnvironmentVariable("APPIMAGE").isEmpty()) {
#if defined(Q_PROCESSOR_ARM_64)
        return QStringLiteral("Vita3K-aarch64.AppImage");
#else
        return QStringLiteral("Vita3K-x86_64.AppImage");
#endif
    }
    if (!QDir(QCoreApplication::applicationDirPath()).exists(QStringLiteral("data")))
        return {};
#if defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("ubuntu-aarch64-latest.zip");
#else
    return QStringLiteral("ubuntu-latest.zip");
#endif
#else
    return {};
#endif
}

std::optional<updater::UpdateAsset> find_platform_asset(const updater::UpdateInfo &info) {
    const QString expected_name = expected_asset_name();
    if (expected_name.isEmpty())
        return std::nullopt;

    return updater::find_asset(info, expected_name.toStdString());
}

#if defined(Q_OS_LINUX)
bool is_valid_appimage(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray header = file.read(20);
    if (header.size() != 20 || static_cast<unsigned char>(header[0]) != 0x7F
        || header[1] != 'E' || header[2] != 'L' || header[3] != 'F'
        || static_cast<unsigned char>(header[4]) != 2 || static_cast<unsigned char>(header[5]) != 1
        || ((static_cast<unsigned char>(header[16]) | (static_cast<unsigned char>(header[17]) << 8)) != 2
            && (static_cast<unsigned char>(header[16]) | (static_cast<unsigned char>(header[17]) << 8)) != 3)
        || header[8] != 'A' || header[9] != 'I'
        || (static_cast<unsigned char>(header[10]) != 1 && static_cast<unsigned char>(header[10]) != 2))
        return false;

    const unsigned short machine = static_cast<unsigned char>(header[18])
        | static_cast<unsigned short>(static_cast<unsigned char>(header[19]) << 8);
#if defined(Q_PROCESSOR_X86_64)
    return machine == 62;
#elif defined(Q_PROCESSOR_ARM_64)
    return machine == 183;
#else
    return false;
#endif
}
#endif

bool extract_update_zip(const QString &archive_path, const QString &destination, QString &error_message, std::atomic_bool *cancellation) {
    mz_zip_archive zip{};
#if defined(Q_OS_WIN)
    const QByteArray archive_path_utf8 = archive_path.toUtf8();
#else
    const QByteArray archive_path_utf8 = QFile::encodeName(archive_path);
#endif
    if (!mz_zip_reader_init_file(&zip, archive_path_utf8.constData(), 0)) {
        error_message = tr_update("The downloaded update archive could not be opened.");
        return false;
    }

    const QString root_path = QDir::fromNativeSeparators(QDir::cleanPath(QFileInfo(destination).absoluteFilePath()));
    const QString root_prefix = root_path.endsWith(QLatin1Char('/')) ? root_path : root_path + QLatin1Char('/');
    bool ok = true;
    const mz_uint file_count = mz_zip_reader_get_num_files(&zip);
    quint64 advertised_output_size = 0;
    for (mz_uint index = 0; index < file_count; ++index) {
        if (cancellation->load(std::memory_order_relaxed)) {
            error_message = tr_update("Update preparation was canceled.");
            mz_zip_reader_end(&zip);
            return false;
        }
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, index, &stat)
            || stat.m_uncomp_size > update_staging_budget - advertised_output_size) {
            error_message = tr_update("The update archive exceeds the supported staging size.");
            mz_zip_reader_end(&zip);
            return false;
        }
        advertised_output_size += stat.m_uncomp_size;
    }
    const QStorageInfo staging_storage(destination);
    if (advertised_output_size > static_cast<quint64>(std::max<qint64>(0, staging_storage.bytesAvailable()))) {
        error_message = tr_update("There is not enough free space to stage this update.");
        mz_zip_reader_end(&zip);
        return false;
    }
    quint64 extracted_output_size = 0;

    for (mz_uint index = 0; index < file_count && ok; ++index) {
        if (cancellation->load(std::memory_order_relaxed)) {
            error_message = tr_update("Update preparation was canceled.");
            ok = false;
            break;
        }
        mz_zip_archive_file_stat file_stat{};
        if (!mz_zip_reader_file_stat(&zip, index, &file_stat) || !file_stat.m_is_supported || file_stat.m_is_encrypted) {
            error_message = tr_update("The update archive contains a file that cannot be extracted.");
            ok = false;
            break;
        }
        const auto mode = static_cast<unsigned int>(file_stat.m_external_attr >> 16);
        if ((mode & 0170000U) == 0120000U) {
            error_message = tr_update("The update archive contains an unsupported symbolic link.");
            ok = false;
            break;
        }

        const mz_uint filename_size = mz_zip_reader_get_filename(&zip, index, nullptr, 0);
        if (filename_size <= 1) {
            error_message = tr_update("The update archive contains an invalid file name.");
            ok = false;
            break;
        }
        QByteArray full_filename(static_cast<int>(filename_size), '\0');
        if (mz_zip_reader_get_filename(&zip, index, full_filename.data(), filename_size) != filename_size) {
            error_message = tr_update("The update archive file name could not be read completely.");
            ok = false;
            break;
        }
        QString relative_path = QString::fromUtf8(full_filename.constData());
        relative_path.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (relative_path.startsWith(QLatin1Char('/')) || relative_path.contains(QLatin1Char(':'))) {
            error_message = tr_update("The update archive contains an unsafe file path.");
            ok = false;
            break;
        }

        QStringList path_parts;
        for (const QString &part : relative_path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
            if (part == QStringLiteral("..")) {
                error_message = tr_update("The update archive contains an unsafe file path.");
                ok = false;
                break;
            }
            if (part != QStringLiteral("."))
                path_parts.append(part);
        }
        if (!ok)
            break;
        if (path_parts.isEmpty())
            continue;

        const QString output_path = QDir::fromNativeSeparators(QDir::cleanPath(QDir(root_path).filePath(path_parts.join(QDir::separator()))));
        const bool stays_in_root = output_path.startsWith(root_prefix,
#if defined(Q_OS_WIN)
            Qt::CaseInsensitive
#else
            Qt::CaseSensitive
#endif
        );
        if (!stays_in_root) {
            error_message = tr_update("The update archive contains an unsafe file path.");
            ok = false;
            break;
        }

        if (file_stat.m_is_directory) {
            ok = QDir().mkpath(output_path);
        } else {
            ok = QDir().mkpath(QFileInfo(output_path).absolutePath());
            if (ok) {
                QFile output_file(output_path);
                ok = output_file.open(QIODevice::WriteOnly | QIODevice::Truncate);
                if (ok) {
                    ZipOutputContext output_context{ &output_file, update_staging_budget - extracted_output_size, cancellation };
                    ok = mz_zip_reader_extract_to_callback(&zip, index, write_zip_output, &output_context, 0);
                    extracted_output_size += update_staging_budget - extracted_output_size - output_context.remaining;
                    output_file.close();
                    if (output_file.error() != QFileDevice::NoError)
                        ok = false;
                }
#if defined(Q_OS_UNIX)
                if (ok) {
                    QFileDevice::Permissions permissions;
                    if ((mode & 0400U) != 0)
                        permissions |= QFileDevice::ReadOwner;
                    if ((mode & 0200U) != 0)
                        permissions |= QFileDevice::WriteOwner;
                    if ((mode & 0100U) != 0)
                        permissions |= QFileDevice::ExeOwner;
                    if ((mode & 0040U) != 0)
                        permissions |= QFileDevice::ReadGroup;
                    if ((mode & 0020U) != 0)
                        permissions |= QFileDevice::WriteGroup;
                    if ((mode & 0010U) != 0)
                        permissions |= QFileDevice::ExeGroup;
                    if ((mode & 0004U) != 0)
                        permissions |= QFileDevice::ReadOther;
                    if ((mode & 0002U) != 0)
                        permissions |= QFileDevice::WriteOther;
                    if ((mode & 0001U) != 0)
                        permissions |= QFileDevice::ExeOther;
                    if (mode == 0)
                        permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther;
                    ok = QFile::setPermissions(output_path, permissions);
                }
#endif
            }
        }

        if (!ok)
            error_message = tr_update("The update files could not be extracted.");
    }

    mz_zip_reader_end(&zip);
    return ok;
}

#if defined(Q_OS_MACOS)
QString macos_app_bundle_path() {
    QString path = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
    while (!path.isEmpty() && path != QDir::rootPath()) {
        if (path.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive))
            return path;
        path = QFileInfo(path).absolutePath();
    }
    return {};
}
#endif

bool is_directory_writable(const QString &path) {
    QTemporaryFile probe(QDir(path).filePath(QStringLiteral(".vita3k-update-XXXXXX")));
    if (!probe.open())
        return false;
    const QString probe_path = probe.fileName();
    probe.close();
    return QFile::remove(probe_path);
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

    std::string effective_url;
    const std::string response = net_utils::get_web_response(updater::release_api_url(), &effective_url, false);
    if (response.empty() || !updater::is_release_api_url(effective_url)) {
        result.status = updater::UpdateCheckStatus::Failed;
        result.message = tr_update("Could not retrieve the latest Vita3K release information. Please try again later.").toStdString();
        return result;
    }

    result = updater::check_release_response(response, static_cast<std::uint64_t>(app_number), updater::is_official_build(), result.current_display_version);
    if (result.status == updater::UpdateCheckStatus::Failed) {
        switch (result.failure_reason) {
        case updater::UpdateCheckFailureReason::InvalidReleaseResponse:
            result.message = tr_update("The GitHub release response could not be parsed.").toStdString();
            break;
        case updater::UpdateCheckFailureReason::InvalidBuildMetadata:
            result.message = tr_update("The latest release does not advertise a usable Vita3K build number.").toStdString();
            break;
        case updater::UpdateCheckFailureReason::None:
            break;
        }
    } else if (result.status == updater::UpdateCheckStatus::CurrentBuildNewerThanLatest)
        result.message = tr_update("This installation appears newer than the latest official continuous build.").toStdString();
    else if (result.status == updater::UpdateCheckStatus::UpToDate)
        result.message = tr_update("You already have the latest official build installed.").toStdString();
    return result;
}

} // namespace

UpdateManager::UpdateManager(std::shared_ptr<GuiSettings> gui_settings, QObject *parent)
    : QObject(parent)
    , m_gui_settings(std::move(gui_settings)) {
    m_original_working_directory = QDir::currentPath();
}

UpdateManager::~UpdateManager() {
    if (m_worker_thread) {
        if (m_preparation_cancel_token)
            m_preparation_cancel_token->store(true, std::memory_order_relaxed);
        m_worker_thread->quit();
        m_worker_thread->wait();
    }
    delete m_download_file;
    delete m_update_directory;
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
    const auto platform_asset = find_platform_asset(result.info);

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
    if (platform_asset)
        buttons.push_back({ QStringLiteral("update"), tr_update("Update"), QMessageBox::AcceptRole, true });
    buttons.push_back({ QStringLiteral("open"), tr_update("Open Download Page"), QMessageBox::ActionRole, !platform_asset.has_value() });
    buttons.push_back({ QStringLiteral("ok"), tr_update("Later"), QMessageBox::RejectRole, false });

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

    if (dialog_result.clicked_id == QStringLiteral("update") && platform_asset) {
        const updater::UpdateAsset asset = *platform_asset;
        if (!m_worker_thread) {
            download_update(asset);
            return;
        }

        QThread *worker_thread = m_worker_thread;
        const auto begin_download = [this, asset]() {
            QTimer::singleShot(0, this, [this, asset]() { download_update(asset); });
        };
        connect(worker_thread, &QThread::finished, this, begin_download, Qt::QueuedConnection);
        if (worker_thread->isFinished())
            begin_download();
    } else if (dialog_result.clicked_id == QStringLiteral("open")) {
        QDesktopServices::openUrl(QUrl(download_url));
    }
}

void UpdateManager::download_update(const updater::UpdateAsset &asset) {
    if (m_download_reply || m_worker_thread)
        return;
    if (m_update_directory && !m_update_mode.isEmpty())
        return;
    if (asset.size == 0 || asset.size > static_cast<std::uint64_t>(std::numeric_limits<qint64>::max())) {
        QMessageBox::warning(m_parent_widget, tr_update("Update Failed"), tr_update("The release package does not have a valid advertised size."));
        return;
    }
    if (m_update_directory) {
        delete m_update_directory;
        m_update_directory = nullptr;
    }
    m_update_mode.clear();

    m_asset_name = QString::fromStdString(asset.name);
    m_expected_download_size = static_cast<qint64>(asset.size);
    m_download_canceled = false;
    m_download_save_error.clear();
    m_update_directory = new QTemporaryDir(QDir(QDir::tempPath()).filePath(QStringLiteral("vita3k-update-XXXXXX")));
    if (!m_update_directory->isValid()) {
        delete m_update_directory;
        m_update_directory = nullptr;
        QMessageBox::warning(m_parent_widget, tr_update("Update Failed"), tr_update("A temporary folder for the update could not be created."));
        return;
    }

    m_download_path = m_update_directory->filePath(m_asset_name);
    m_download_file = new QFile(m_download_path, this);
    if (!m_download_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete m_download_file;
        m_download_file = nullptr;
        delete m_update_directory;
        m_update_directory = nullptr;
        QMessageBox::warning(m_parent_widget, tr_update("Update Failed"), tr_update("The update package could not be saved."));
        return;
    }

    auto *dialog = new QProgressDialog(tr_update("Downloading update..."), tr_update("Cancel"), 0, 1000, m_parent_widget);
    dialog->setWindowTitle(tr_update("Downloading Update"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setMinimumDuration(0);
    dialog->setAutoClose(false);
    dialog->setAutoReset(false);
    connect(dialog, &QProgressDialog::canceled, this, [this]() {
        if (m_download_reply) {
            m_download_canceled = true;
            m_download_reply->abort();
        } else if (m_worker_thread) {
            if (m_preparation_cancel_token)
                m_preparation_cancel_token->store(true, std::memory_order_relaxed);
        }
    });
    m_progress_dialog = dialog;
    dialog->show();

    m_download_redirect_count = 0;
    start_download_request(QUrl(QString::fromStdString(asset.download_url)));
}

void UpdateManager::start_download_request(const QUrl &url) {
    QNetworkRequest request{ url };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Vita3K Updater"));
    request.setTransferTimeout(60000);
    m_download_reply = m_network.get(request);
    connect(m_download_reply, &QIODevice::readyRead, this, &UpdateManager::receive_download_data);
    connect(m_download_reply, &QNetworkReply::downloadProgress, this, &UpdateManager::update_download_progress);
    connect(m_download_reply, &QNetworkReply::finished, this, [this]() {
        if (!m_download_reply)
            return;
        if (m_download_canceled || (m_progress_dialog && m_progress_dialog->wasCanceled())) {
            finish_download();
            return;
        }

        const QVariant redirect = m_download_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (!redirect.isValid()) {
            finish_download();
            return;
        }

        const QUrl target = m_download_reply->url().resolved(redirect.toUrl());
        const QString host = target.host().toLower();
        const bool approved_host = host == QStringLiteral("github.com")
            || host == QStringLiteral("release-assets.githubusercontent.com")
            || host == QStringLiteral("objects.githubusercontent.com");
        const bool approved_url = target.scheme() == QStringLiteral("https") && approved_host
            && target.userInfo().isEmpty() && (target.port() == -1 || target.port() == 443)
            && (host != QStringLiteral("github.com") || updater::is_release_asset_url(target.toString().toStdString()));
        if (!approved_url || m_download_redirect_count >= 5) {
            m_download_save_error = tr_update("The update download redirected to an untrusted location.");
            finish_download();
            return;
        }

        ++m_download_redirect_count;
        m_download_reply->deleteLater();
        start_download_request(target);
    });
}

void UpdateManager::receive_download_data() {
    if (!m_download_reply || !m_download_file)
        return;

    const QByteArray data = m_download_reply->readAll();
    if (m_download_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid())
        return;
    if (!data.isEmpty() && m_expected_download_size > 0
        && data.size() > m_expected_download_size - m_download_file->size()) {
        m_download_save_error = tr_update("The downloaded update exceeded its expected size.");
        m_download_reply->abort();
        if (m_progress_dialog)
            m_progress_dialog->setLabelText(m_download_save_error);
        return;
    }
    if (!data.isEmpty() && m_download_file->write(data) != data.size()) {
        m_download_save_error = tr_update("Could not save the downloaded update.");
        m_download_reply->abort();
        if (m_progress_dialog)
            m_progress_dialog->setLabelText(m_download_save_error);
    }
}

void UpdateManager::update_download_progress(const qint64 received, const qint64 total) {
    if (!m_progress_dialog)
        return;

    if (total <= 0) {
        m_progress_dialog->setRange(0, 0);
        return;
    }

    m_progress_dialog->setRange(0, 1000);
    m_progress_dialog->setLabelText(tr_update("Downloading update... %1 MB of %2 MB")
            .arg(received / (1024 * 1024))
            .arg(total / (1024 * 1024)));
    m_progress_dialog->setValue(static_cast<int>(std::min<qint64>(1000, (received * 1000) / total)));
}

void UpdateManager::finish_download() {
    receive_download_data();
    if (m_download_file && !m_download_file->flush() && m_download_save_error.isEmpty())
        m_download_save_error = tr_update("Could not save the downloaded update.");
    const bool download_canceled = m_download_canceled;
    const QString save_error = m_download_save_error;
    m_download_canceled = false;
    m_download_save_error.clear();
    const QNetworkReply::NetworkError network_error = m_download_reply ? m_download_reply->error() : QNetworkReply::UnknownNetworkError;
    const QString network_error_text = m_download_reply ? m_download_reply->errorString() : tr_update("The download failed.");
    const int http_status = m_download_reply ? m_download_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0;
    if (m_download_reply) {
        m_download_reply->deleteLater();
        m_download_reply = nullptr;
    }

    if (m_download_file) {
        m_download_file->close();
        delete m_download_file;
        m_download_file = nullptr;
    }

    if (download_canceled) {
        close_progress_dialog();
        if (m_update_directory) {
            delete m_update_directory;
            m_update_directory = nullptr;
        }
        return;
    }

    if (!save_error.isEmpty()) {
        close_progress_dialog();
        QMessageBox::warning(m_parent_widget, tr_update("Update Failed"), save_error);
        if (m_update_directory) {
            delete m_update_directory;
            m_update_directory = nullptr;
        }
        return;
    }

    if (network_error != QNetworkReply::NoError) {
        close_progress_dialog();
        QMessageBox::warning(m_parent_widget, tr_update("Update Failed"), tr_update("The update could not be downloaded: %1").arg(network_error_text));
        if (m_update_directory) {
            delete m_update_directory;
            m_update_directory = nullptr;
        }
        return;
    }

    if (http_status != 200) {
        close_progress_dialog();
        QMessageBox::warning(m_parent_widget, tr_update("Update Failed"), tr_update("The update server returned an unexpected HTTP status: %1").arg(http_status));
        if (m_update_directory) {
            delete m_update_directory;
            m_update_directory = nullptr;
        }
        return;
    }

    if (m_progress_dialog && m_progress_dialog->wasCanceled()) {
        close_progress_dialog();
        if (m_update_directory) {
            delete m_update_directory;
            m_update_directory = nullptr;
        }
        return;
    }

    if (m_progress_dialog) {
        m_preparation_cancel_token = std::make_shared<std::atomic_bool>(false);
        m_progress_dialog->setLabelText(tr_update("Preparing update files..."));
        m_progress_dialog->setRange(0, 0);
        m_progress_dialog->setCancelButton(nullptr);
    }

    struct PreparationResult {
        bool succeeded = false;
        QString error_message;
    };
    auto preparation = std::make_shared<PreparationResult>();
    const auto cancellation = m_preparation_cancel_token;
    QThread *preparation_thread = QThread::create([this, preparation, cancellation]() {
        preparation->succeeded = prepare_update(preparation->error_message, cancellation);
    });
    preparation_thread->setParent(this);
    m_worker_thread = preparation_thread;
    connect(preparation_thread, &QThread::finished, this, [this, preparation_thread, preparation, cancellation]() {
        if (m_worker_thread == preparation_thread)
            m_worker_thread = nullptr;
        preparation_thread->deleteLater();

        if (m_preparation_cancel_token == cancellation)
            m_preparation_cancel_token.reset();
        if (cancellation->load(std::memory_order_relaxed) || (m_progress_dialog && m_progress_dialog->wasCanceled())) {
            close_progress_dialog();
            if (m_update_directory) {
                delete m_update_directory;
                m_update_directory = nullptr;
            }
            m_update_mode.clear();
            return;
        }

        if (!preparation->succeeded) {
            close_progress_dialog();
            QMessageBox::warning(m_parent_widget, tr_update("Update Failed"), preparation->error_message);
            if (m_update_directory) {
                delete m_update_directory;
                m_update_directory = nullptr;
            }
            return;
        }

        close_progress_dialog();
        if (m_gui_settings && m_gui_settings->get_value(gui::um_dontShowUpdateComplete).toBool())
            restart_with_update();
        else
            show_update_complete();
    }, Qt::QueuedConnection);
    preparation_thread->start();
}

bool UpdateManager::prepare_update(QString &error_message, const std::shared_ptr<std::atomic_bool> &cancellation) {
    const qint64 downloaded_size = QFileInfo(m_download_path).size();
    if (!m_update_directory || !m_update_directory->isValid() || downloaded_size <= 0
        || (m_expected_download_size > 0 && downloaded_size != m_expected_download_size)) {
        error_message = tr_update("The downloaded update package is empty or incomplete.");
        return false;
    }

    if (m_asset_name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
        m_update_mode = QStringLiteral("zip");
        m_install_path = QCoreApplication::applicationDirPath();
        m_stage_path = m_update_directory->filePath(QStringLiteral("staged"));
        if (!QDir().mkpath(m_stage_path)) {
            error_message = tr_update("The update staging directory could not be created.");
            return false;
        }
        if (!extract_update_zip(m_download_path, m_stage_path, error_message, cancellation.get()))
            return false;
        if (m_install_path.contains(QLatin1Char('\n')) || m_install_path.contains(QLatin1Char('\r'))) {
            error_message = tr_update("The install path contains a line break and cannot be updated safely.");
            return false;
        }
        QDirIterator staged_entries(m_stage_path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
        while (staged_entries.hasNext()) {
            const QString relative_path = QDir(m_stage_path).relativeFilePath(staged_entries.next());
            if (relative_path.contains(QLatin1Char('\n')) || relative_path.contains(QLatin1Char('\r'))) {
                error_message = tr_update("The update archive contains a path with a line break and cannot be installed safely.");
                return false;
            }
        }
        const QString staged_executable_path = QDir(m_stage_path).filePath(QFileInfo(QCoreApplication::applicationFilePath()).fileName());
        if (!QFileInfo(staged_executable_path).isFile()) {
            error_message = tr_update("The update archive does not contain the Vita3K executable.");
            return false;
        }
#if defined(Q_OS_LINUX)
        QFile staged_executable(staged_executable_path);
        if (!staged_executable.setPermissions(staged_executable.permissions()
                | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther)) {
            error_message = tr_update("The updated Vita3K executable could not be prepared to run.");
            return false;
        }
#endif
    } else if (m_asset_name.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive)) {
#if defined(Q_OS_LINUX)
        if (!is_valid_appimage(m_download_path)) {
            error_message = tr_update("The downloaded AppImage is invalid or does not match this system's architecture.");
            return false;
        }
        m_update_mode = QStringLiteral("appimage");
        m_install_path = qEnvironmentVariable("APPIMAGE");
        if (m_install_path.isEmpty()) {
            error_message = tr_update("The AppImage location could not be found.");
            return false;
        }
#else
        error_message = tr_update("This update package cannot be installed on this platform.");
        return false;
#endif
    } else if (m_asset_name.endsWith(QStringLiteral(".dmg"), Qt::CaseInsensitive)) {
#if defined(Q_OS_MACOS)
        m_update_mode = QStringLiteral("dmg");
        m_install_path = macos_app_bundle_path();
        if (m_install_path.isEmpty()) {
            error_message = tr_update("The Vita3K app location could not be found inside the macOS app bundle.");
            return false;
        }
#else
        error_message = tr_update("This update package cannot be installed on this platform.");
        return false;
#endif
    } else {
        error_message = tr_update("The release package type is not supported for automatic updates.");
        return false;
    }

    QString writable_directory = m_install_path;
    if (m_update_mode == QStringLiteral("zip")) {
        writable_directory = QCoreApplication::applicationDirPath();
    } else {
        writable_directory = QFileInfo(m_install_path).absolutePath();
    }
    if (!is_directory_writable(writable_directory) && m_update_mode != QStringLiteral("dmg")) {
        error_message = tr_update("Vita3K cannot write to its install folder. Open the release page to install this update manually.");
        return false;
    }

    return true;
}

void UpdateManager::show_update_complete() {
    QDialog dialog(m_parent_widget);
    dialog.setWindowTitle(tr_update("Update Complete"));
    dialog.setWindowModality(Qt::WindowModal);

    auto *layout = new QVBoxLayout(&dialog);
    auto *label = new QLabel(tr_update("The update is ready. Restart Vita3K to finish installing it."), &dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    auto *actions = new QHBoxLayout();
    auto *restart_button = new QPushButton(tr_update("Restart"), &dialog);
    auto *dont_show_again = new QCheckBox(tr_update("Don't show this again"), &dialog);
    actions->addWidget(dont_show_again);
    actions->addStretch();
    actions->addWidget(restart_button);
    layout->addLayout(actions);

    connect(restart_button, &QPushButton::clicked, &dialog, [this, &dialog, dont_show_again]() {
        if (dont_show_again->isChecked()) {
            if (m_gui_settings)
                m_gui_settings->set_value(gui::um_dontShowUpdateComplete, true);
        }
        dialog.accept();
    });

    if (dialog.exec() == QDialog::Accepted) {
        restart_with_update();
    } else if (m_update_directory) {
        delete m_update_directory;
        m_update_directory = nullptr;
    }
}

void UpdateManager::restart_with_update() {
    if (!m_update_directory)
        return;
    const auto release_prepared_update = [this]() {
        if (m_update_directory) {
            m_update_directory->setAutoRemove(true);
            delete m_update_directory;
            m_update_directory = nullptr;
        }
        m_stage_path.clear();
        m_update_mode.clear();
    };

    const QString process_id = QString::number(QCoreApplication::applicationPid());
    const QString executable_path = QCoreApplication::applicationFilePath();
    QString restart_executable = executable_path;
#if defined(Q_OS_LINUX)
    if (m_update_mode == QStringLiteral("appimage"))
        restart_executable = m_install_path;
#endif
    const QString arguments_path = m_update_directory->filePath(QStringLiteral("restart-arguments.json"));
    QJsonArray restart_arguments;
    const QStringList arguments = QCoreApplication::arguments().mid(1);
    for (const QString &argument : arguments)
        restart_arguments.append(argument);
    QSaveFile arguments_file(arguments_path);
    if (!arguments_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(m_parent_widget, tr_update("Restart Failed"), tr_update("The restart information could not be prepared."));
        release_prepared_update();
        return;
    }
    const QByteArray restart_arguments_json = QJsonDocument(restart_arguments).toJson(QJsonDocument::Compact);
    if (arguments_file.write(restart_arguments_json) != restart_arguments_json.size() || !arguments_file.commit()) {
        QMessageBox::warning(m_parent_widget, tr_update("Restart Failed"), tr_update("The restart information could not be saved."));
        release_prepared_update();
        return;
    }
    const QString cancel_path = m_update_directory->filePath(QStringLiteral("cancel-update"));
    const QString cancel_placeholder_path = m_update_directory->filePath(QStringLiteral("cancel-update-placeholder"));
    const QString approval_path = m_update_directory->filePath(QStringLiteral("shutdown-approved"));
    QFile cancel_placeholder(cancel_placeholder_path);
    if (!cancel_placeholder.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(m_parent_widget, tr_update("Restart Failed"), tr_update("The update shutdown handoff could not be prepared."));
        release_prepared_update();
        return;
    }
    cancel_placeholder.close();
    if (cancel_placeholder.error() != QFileDevice::NoError) {
        QMessageBox::warning(m_parent_widget, tr_update("Restart Failed"), tr_update("The update shutdown handoff could not be saved."));
        release_prepared_update();
        return;
    }
    QString helper_program;
    QStringList helper_arguments;

#if defined(Q_OS_WIN)
    const QString script_path = m_update_directory->filePath(QStringLiteral("install-update.ps1"));
    QSaveFile script(script_path);
    const QByteArray script_contents = R"powershell(
param([int]$TargetPid, [string]$Mode, [string]$Payload, [string]$InstallPath, [string]$Executable, [string]$OriginalWorkingDirectory, [string]$TempPath, [string]$ArgumentsPath, [string]$CancelPath, [string]$ApprovalPath)
$ErrorActionPreference = 'Stop'
function ConvertTo-WindowsCommandLineArgument([string]$Argument) {
    $Builder = New-Object System.Text.StringBuilder
    [void]$Builder.Append([char]34)
    $BackslashCount = 0
    foreach ($Character in $Argument.ToCharArray()) {
        if ($Character -eq [char]92) {
            $BackslashCount++
            continue
        }
        if ($Character -eq [char]34) {
            for ($Index = 0; $Index -lt (2 * $BackslashCount + 1); $Index++) { [void]$Builder.Append([char]92) }
            [void]$Builder.Append([char]34)
            $BackslashCount = 0
            continue
        }
        for ($Index = 0; $Index -lt $BackslashCount; $Index++) { [void]$Builder.Append([char]92) }
        $BackslashCount = 0
        [void]$Builder.Append($Character)
    }
    for ($Index = 0; $Index -lt (2 * $BackslashCount); $Index++) { [void]$Builder.Append([char]92) }
    [void]$Builder.Append([char]34)
    return $Builder.ToString()
}
function Start-Vita3K([string]$Program, [string]$WorkingDirectory, [string[]]$Arguments) {
    $QuotedArguments = @($Arguments | ForEach-Object { ConvertTo-WindowsCommandLineArgument ([string]$_) })
    $StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $StartInfo.FileName = $Program
    $StartInfo.WorkingDirectory = $WorkingDirectory
    $StartInfo.UseShellExecute = $false
    $StartInfo.Arguments = [string]::Join(' ', [string[]]$QuotedArguments)
    $Process = [System.Diagnostics.Process]::Start($StartInfo)
    if ($null -eq $Process) { throw 'Vita3K could not be started.' }
    try {
        if (-not $Process.WaitForInputIdle(10000)) { throw 'Vita3K did not finish starting.' }
    } catch {
        if (-not $Process.HasExited) {
            $Process.Kill()
            $Process.WaitForExit()
        }
        throw
    }
    return $Process
}
function Restore-ZipInstall([string]$RootPath, [string]$BackupFilesPath, [string]$TransactionPath, $InstalledFiles, $BackedUpFiles) {
    $RollbackFailed = $false
    for ($Index = $InstalledFiles.Count - 1; $Index -ge 0; $Index--) {
        try {
            $Destination = Join-Path $RootPath $InstalledFiles[$Index]
            if (Test-Path -LiteralPath $Destination) {
                if (Test-Path -LiteralPath $Destination -PathType Container) { throw 'A destination directory appeared during rollback.' }
                Remove-Item -LiteralPath $Destination -Force
            }
        } catch {
            $RollbackFailed = $true
        }
    }
    for ($Index = $BackedUpFiles.Count - 1; $Index -ge 0; $Index--) {
        try {
            $RelativePath = $BackedUpFiles[$Index]
            $Backup = Join-Path $BackupFilesPath $RelativePath
            if (Test-Path -LiteralPath $Backup) {
                $Destination = Join-Path $RootPath $RelativePath
                if (Test-Path -LiteralPath $Destination) { throw 'A destination is occupied during rollback.' }
                New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
                [System.IO.File]::Move($Backup, $Destination)
            }
        } catch {
            $RollbackFailed = $true
        }
    }
    if (-not $RollbackFailed) {
        Remove-Item -LiteralPath $TransactionPath -Recurse -Force -ErrorAction SilentlyContinue
    }
    return $RollbackFailed
}
$TransactionPath = $null
$BackupFilesPath = $null
$InstalledFiles = New-Object 'System.Collections.Generic.List[string]'
$BackedUpFiles = New-Object 'System.Collections.Generic.List[string]'
$RollbackFailed = $false
$ZipFilesCommitted = $false
$AppArguments = @()
try {
    while (Get-Process -Id $TargetPid -ErrorAction SilentlyContinue) {
        if (Test-Path -LiteralPath $CancelPath) {
            Remove-Item -LiteralPath $TempPath -Recurse -Force
            exit 0
        }
        Start-Sleep -Milliseconds 250
    }
    if (Test-Path -LiteralPath $CancelPath) {
        Remove-Item -LiteralPath $TempPath -Recurse -Force
        exit 0
    }
    if (-not (Test-Path -LiteralPath $ApprovalPath)) {
        Remove-Item -LiteralPath $TempPath -Recurse -Force
        exit 0
    }
    $ArgumentJson = [System.IO.File]::ReadAllText($ArgumentsPath, [System.Text.Encoding]::UTF8)
    $TrimmedArgumentJson = $ArgumentJson.Trim()
    if (-not $TrimmedArgumentJson.StartsWith('[') -or -not $TrimmedArgumentJson.EndsWith(']')) {
        throw 'The restart arguments are not a JSON array.'
    }
    $AppArguments = ConvertFrom-Json -InputObject $ArgumentJson -ErrorAction Stop
    if ($AppArguments -isnot [System.Array]) {
        throw 'The restart arguments are not a JSON array.'
    }
    foreach ($Argument in $AppArguments) {
        if ($null -eq $Argument -or $Argument -isnot [string]) {
            throw 'The restart arguments contain an invalid value.'
        }
    }
    $AppArguments = [string[]]$AppArguments
    if ($Mode -eq 'zip') {
        $TransactionPath = Join-Path $InstallPath ('.vita3k-update-' + [Guid]::NewGuid().ToString('N'))
        $NewFilesPath = Join-Path $TransactionPath 'new'
        $BackupFilesPath = Join-Path $TransactionPath 'backup'
        try {
            New-Item -ItemType Directory -Path $NewFilesPath, $BackupFilesPath -Force | Out-Null
            Get-ChildItem -LiteralPath $Payload -Force | ForEach-Object {
                Copy-Item -LiteralPath $_.FullName -Destination $NewFilesPath -Recurse -Force
            }
            $StagedFiles = @(Get-ChildItem -LiteralPath $NewFilesPath -File -Recurse)
            foreach ($StagedFile in $StagedFiles) {
                $RelativePath = $StagedFile.FullName.Substring($NewFilesPath.Length).TrimStart([char]92, [char]47)
                $Destination = Join-Path $InstallPath $RelativePath
                $Ancestor = Split-Path -Parent $Destination
                while ($Ancestor -and $Ancestor.StartsWith($InstallPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                    if (Test-Path -LiteralPath $Ancestor) {
                        $AncestorItem = Get-Item -LiteralPath $Ancestor -Force
                        if (($AncestorItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -and $AncestorItem.PSIsContainer) {
                            throw "The update cannot use a redirected destination directory: $Ancestor"
                        }
                    }
                    if ($Ancestor -eq $InstallPath) { break }
                    $Ancestor = Split-Path -Parent $Ancestor
                }
                if ((Test-Path -LiteralPath $Destination) -and (Test-Path -LiteralPath $Destination -PathType Container)) {
                    throw "The update cannot replace an existing directory: $Destination"
                }
            }
            foreach ($StagedFile in $StagedFiles) {
                $RelativePath = $StagedFile.FullName.Substring($NewFilesPath.Length).TrimStart([char]92, [char]47)
                $Destination = Join-Path $InstallPath $RelativePath
                $Backup = Join-Path $BackupFilesPath $RelativePath
                New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
                if (Test-Path -LiteralPath $Destination) {
                    if (Test-Path -LiteralPath $Destination -PathType Container) {
                        throw "The update cannot replace an existing directory: $Destination"
                    }
                    New-Item -ItemType Directory -Path (Split-Path -Parent $Backup) -Force | Out-Null
                    [System.IO.File]::Move($Destination, $Backup)
                    [void]$BackedUpFiles.Add($RelativePath)
                }
                [System.IO.File]::Move($StagedFile.FullName, $Destination)
                [void]$InstalledFiles.Add($RelativePath)
            }
            $ZipFilesCommitted = $true
        } catch {
            $RollbackFailed = Restore-ZipInstall $InstallPath $BackupFilesPath $TransactionPath $InstalledFiles $BackedUpFiles
            throw
        }
    }
    $RestartedProcess = Start-Vita3K $Executable $OriginalWorkingDirectory $AppArguments
    if ($null -eq $RestartedProcess) { throw 'Vita3K could not be started after the update.' }
    $ZipFilesCommitted = $false
    if ($TransactionPath) {
        Remove-Item -LiteralPath $TransactionPath -Recurse -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Milliseconds 500
    Remove-Item -LiteralPath $TempPath -Recurse -Force -ErrorAction SilentlyContinue
} catch {
    $FailureMessage = $_.Exception.Message
    if ($ZipFilesCommitted -and $TransactionPath -and (Test-Path -LiteralPath $TransactionPath)) {
        $RollbackFailed = Restore-ZipInstall $InstallPath $BackupFilesPath $TransactionPath $InstalledFiles $BackedUpFiles
        $ZipFilesCommitted = $false
    }
    if ($RollbackFailed) {
        $UserMessage = "The update failed and rollback was incomplete. Vita3K was not started. Recovery files are at: $TransactionPath`n`n$FailureMessage"
    } else {
        $RestartError = ''
        $RestartSucceeded = $false
        try {
            $PreviousProcess = Start-Vita3K $Executable $OriginalWorkingDirectory $AppArguments
            if ($null -eq $PreviousProcess) { throw 'Vita3K could not be started.' }
            $RestartSucceeded = $true
        } catch {
            $RestartError = "`n`nVita3K could not be reopened: $($_.Exception.Message)"
        }
        if (-not $RestartSucceeded) {
            $UserMessage = "The update was not applied. Vita3K must be started manually.`n`n$FailureMessage$RestartError"
        } else {
            $UserMessage = "The update could not be installed. Vita3K was reopened with the previous build.`n`n$FailureMessage"
        }
    }
    $MessageShown = $false
    try {
        Add-Type -AssemblyName System.Windows.Forms
        [System.Windows.Forms.MessageBox]::Show($UserMessage, 'Vita3K Update Failed', [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error) | Out-Null
        $MessageShown = $true
    } catch {
        [System.IO.File]::WriteAllText((Join-Path $TempPath 'install-error.txt'), $UserMessage)
    }
    if (-not $RollbackFailed -and $MessageShown) {
        Remove-Item -LiteralPath $TempPath -Recurse -Force -ErrorAction SilentlyContinue
    }
}
)powershell";
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || script.write(script_contents) != script_contents.size()
        || !script.commit()) {
        QMessageBox::warning(m_parent_widget, tr_update("Restart Failed"), tr_update("The update installer could not be started."));
        release_prepared_update();
        return;
    }

    const QString payload = m_update_mode == QStringLiteral("zip") ? m_stage_path : m_download_path;
    helper_program = QStringLiteral("powershell.exe");
    helper_arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-WindowStyle"),
        QStringLiteral("Hidden"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-File"),
        script_path,
        QStringLiteral("-TargetPid"),
        process_id,
        QStringLiteral("-Mode"),
        m_update_mode,
        QStringLiteral("-Payload"),
        payload,
        QStringLiteral("-InstallPath"),
        m_install_path,
        QStringLiteral("-Executable"),
        executable_path,
        QStringLiteral("-OriginalWorkingDirectory"),
        m_original_working_directory,
        QStringLiteral("-TempPath"),
        m_update_directory->path(),
        QStringLiteral("-ArgumentsPath"),
        arguments_path,
        QStringLiteral("-CancelPath"),
        cancel_path,
        QStringLiteral("-ApprovalPath"),
        approval_path,
    };
#else
    const QString script_path = m_update_directory->filePath(QStringLiteral("install-update.sh"));
    QSaveFile script(script_path);
    QString script_contents = QStringLiteral(
        "#!/bin/sh\n"
        "set -eu\n"
        "pid=\"$1\"; mode=\"$2\"; payload=\"$3\"; install_path=\"$4\"; executable=\"$5\"; temp_path=\"$6\"; cancel_path=\"$7\"; approval_path=\"$8\"; shift 8\n"
        "if [ -e \"$cancel_path\" ]; then rm -rf \"$temp_path\"; exit 0; fi\n"
        "while kill -0 \"$pid\" 2>/dev/null; do if [ -e \"$cancel_path\" ]; then rm -rf \"$temp_path\"; exit 0; fi; sleep 0.25; done\n"
        "if [ -e \"$cancel_path\" ]; then rm -rf \"$temp_path\"; exit 0; fi\n"
        "if [ ! -e \"$approval_path\" ]; then rm -rf \"$temp_path\"; exit 0; fi\n"
        "transaction_path=; transaction_dir=; dmg_backup=; dmg_replaced=0; dmg_elevated=0; install_committed=0; mount_path=; rollback_failed=0; appimage_transaction=; appimage_backup=; appimage_staging=; appimage_replaced=0; recovery_detail=\n"
        "cleanup_mount() { if [ -n \"$mount_path\" ]; then hdiutil detach -quiet \"$mount_path\" >/dev/null 2>&1 || true; rmdir \"$mount_path\" >/dev/null 2>&1 || true; mount_path=; fi; }\n"
        "show_update_error() { if [ \"$(uname -s)\" = Darwin ]; then /usr/bin/osascript -e 'on run argv' -e 'display dialog (item 1 of argv) buttons {\"OK\"} with icon stop' -e 'end run' \"$1\" >/dev/null 2>&1 || true; elif command -v zenity >/dev/null 2>&1; then zenity --error --text=\"$1\" >/dev/null 2>&1 || true; elif command -v kdialog >/dev/null 2>&1; then kdialog --error \"$1\" >/dev/null 2>&1 || true; elif command -v xmessage >/dev/null 2>&1; then xmessage -center \"$1\" >/dev/null 2>&1 || true; elif command -v notify-send >/dev/null 2>&1; then notify-send 'Vita3K Update Failed' \"$1\" >/dev/null 2>&1 || true; else printf '%s\\n' \"$1\" >&2; fi; }\n"
        "process_is_running() { process_state=\"$(ps -o stat= -p \"$1\" 2>/dev/null || true)\"; case \"$process_state\" in *Z*) return 1 ;; esac; [ -n \"$process_state\" ]; }\n"
        "wait_for_startup() { startup_pid=\"$1\"; startup_attempt=0; while [ \"$startup_attempt\" -lt 20 ]; do sleep 0.25; if ! process_is_running \"$startup_pid\"; then return 1; fi; startup_attempt=$((startup_attempt + 1)); done; return 0; }\n"
        "remove_dmg_transaction() { if [ \"$dmg_elevated\" -eq 1 ]; then /usr/bin/osascript -e 'on run argv' -e 'do shell script \"/bin/rm -rf \" & quoted form of (item 1 of argv) with administrator privileges' -e 'end run' \"$transaction_dir\"; else rm -rf \"$transaction_dir\"; fi; }\n"
        "restore_dmg() { if [ \"$dmg_elevated\" -eq 1 ]; then /usr/bin/osascript -e 'on run argv' -e 'set commandText to \"if [ -e \" & quoted form of (item 2 of argv) & \" ] || [ -L \" & quoted form of (item 2 of argv) & \" ]; then /bin/rm -rf \" & quoted form of (item 1 of argv) & \" && /bin/mv \" & quoted form of (item 2 of argv) & \" \" & quoted form of (item 1 of argv) & \" && /bin/rm -rf \" & quoted form of (item 3 of argv) & \"; else exit 1; fi\"' -e 'do shell script commandText with administrator privileges' -e 'end run' \"$install_path\" \"$dmg_backup\" \"$transaction_dir\"; else if [ -e \"$dmg_backup\" ] || [ -L \"$dmg_backup\" ]; then rm -rf \"$install_path\" && mv \"$dmg_backup\" \"$install_path\" && rm -rf \"$transaction_dir\"; else return 1; fi; fi; }\n"
        "handle_update_failure() {\n"
        "  status=$?; trap - EXIT; cleanup_mount; [ \"$status\" -ne 0 ] || exit 0\n"
        "  if [ \"$install_committed\" -eq 1 ]; then message=\"Vita3K started, but update cleanup failed. Retained recovery files: $transaction_path ${appimage_transaction} $transaction_dir $temp_path\"; show_update_error \"$message\"; exit \"$status\"; fi\n"
        "  if [ -n \"$appimage_backup\" ] && { [ -e \"$appimage_backup\" ] || [ -L \"$appimage_backup\" ]; }; then if [ \"$appimage_replaced\" -eq 1 ] && { [ -e \"$install_path\" ] || [ -L \"$install_path\" ]; }; then rm -f \"$install_path\" || rollback_failed=1; fi; if [ ! -e \"$install_path\" ] && [ ! -L \"$install_path\" ]; then mv \"$appimage_backup\" \"$install_path\" || rollback_failed=1; fi; fi\n"
        "  if [ -n \"$appimage_transaction\" ] && [ \"$rollback_failed\" -eq 0 ] && [ ! -e \"$appimage_backup\" ] && [ ! -L \"$appimage_backup\" ]; then rm -rf \"$appimage_transaction\" || rollback_failed=1; fi\n"
        "  if [ -n \"$dmg_backup\" ]; then if ! restore_dmg; then rollback_failed=1; fi; fi\n"
        "  transaction_cleanup_failed=0\n"
        "  if [ -n \"$transaction_path\" ] && [ \"$rollback_failed\" -eq 0 ]; then if rm -rf \"$transaction_path\"; then transaction_path=; else transaction_cleanup_failed=1; fi; fi\n"
        "  if [ -n \"$transaction_dir\" ] && [ \"$rollback_failed\" -eq 0 ] && [ \"$mode\" != dmg ]; then rm -rf \"$transaction_dir\"; fi\n"
        "  if [ -n \"$transaction_dir\" ] && [ \"$mode\" = dmg ] && [ \"$dmg_elevated\" -eq 0 ] && [ ! -e \"$dmg_backup\" ] && [ \"$rollback_failed\" -eq 0 ]; then rm -rf \"$transaction_dir\"; fi\n"
        "  if [ \"$rollback_failed\" -ne 0 ]; then message=\"The update failed and rollback was incomplete. Recovery files are at: $transaction_path ${appimage_transaction} $recovery_detail $transaction_dir\"; else recovery_started=0; if [ \"$mode\" = dmg ]; then if open \"$install_path\" --args \"$@\" >/dev/null 2>&1; then recovery_started=1; fi; else \"$executable\" \"$@\" >/dev/null 2>&1 & recovery_pid=$!; if wait_for_startup \"$recovery_pid\"; then recovery_started=1; else wait \"$recovery_pid\" || true; fi; fi; if [ \"$recovery_started\" -eq 1 ]; then message=\"The update could not be installed. Vita3K was reopened with the previous build.\"; else message=\"The update was not applied. Vita3K must be started manually.\"; fi; fi\n"
        "  if [ \"$rollback_failed\" -eq 0 ] && ! rm -rf \"$temp_path\"; then temp_cleanup_failed=1; else temp_cleanup_failed=0; fi\n"
        "  show_update_error \"$message\"\n"
        "  if [ \"$transaction_cleanup_failed\" -eq 1 ]; then show_update_error \"Update transaction files were retained at: $transaction_path\"; fi\n"
        "  if [ \"$temp_cleanup_failed\" -eq 1 ]; then show_update_error \"Update temporary files could not be removed: $temp_path\"; fi\n"
        "  exit \"$status\"\n"
        "}\n"
        "trap 'handle_update_failure \"$@\"' EXIT\n"
        "case \"$mode\" in\n"
        "  zip)\n"
        "    transaction_path=\"$(mktemp -d \"$install_path/.vita3k-update.XXXXXX\")\"\n"
        "    new_root=\"$transaction_path/new\"; backup_root=\"$transaction_path/backup\"\n"
        "    installed_log=\"$transaction_path/installed\"; backup_log=\"$transaction_path/backed-up\"; executable_replaced=0; executable_identity=; executable_original_identity=; executable_backup=\n"
        "    mkdir -p \"$new_root\" \"$backup_root\"; : > \"$installed_log\"; : > \"$backup_log\"\n"
        "    if ! cp -R \"$payload/.\" \"$new_root/\"; then rm -rf \"$transaction_path\"; exit 1; fi\n"
        "    if ! find \"$new_root\" -type f -print > \"$transaction_path/files\"; then rm -rf \"$transaction_path\"; exit 1; fi\n"
        "    while IFS= read -r staged_file; do\n"
        "      relative_path=${staged_file#\"$new_root\"/}; destination=\"$install_path/$relative_path\"; parent=\"$(dirname \"$destination\")\"\n"
        "      while [ \"$parent\" != \"$install_path\" ] && [ \"$parent\" != / ]; do if [ -L \"$parent\" ]; then printf '%s\\n' \"The update cannot use a symlinked install directory: $parent\" >&2; rm -rf \"$transaction_path\"; exit 1; fi; parent=\"$(dirname \"$parent\")\"; done\n"
        "      if [ -d \"$destination\" ] && [ ! -L \"$destination\" ]; then printf '%s\\n' \"The update cannot replace an existing directory: $destination\" >&2; rm -rf \"$transaction_path\"; exit 1; fi\n"
        "    done < \"$transaction_path/files\"\n"
        "    rollback_zip() {\n"
        "      rollback_failed=0\n"
        "      if [ \"$executable_replaced\" -eq 1 ] && { [ -e \"$executable\" ] || [ -L \"$executable\" ]; }; then current_identity=\"$(stat -c '%d:%i' -- \"$executable\" 2>/dev/null || true)\"; if [ \"$current_identity\" = \"$executable_identity\" ]; then rm -f \"$executable\" || rollback_failed=1; else rollback_failed=1; fi; fi\n"
        "      while IFS= read -r relative_path && IFS= read -r installed_identity; do destination=\"$install_path/$relative_path\"; backup=\"$backup_root/$relative_path\"; if [ -e \"$destination\" ] || [ -L \"$destination\" ]; then current_identity=\"$(stat -c '%d:%i' -- \"$destination\" 2>/dev/null || true)\"; if [ \"$current_identity\" = \"$installed_identity\" ]; then rm -f \"$destination\" || rollback_failed=1; elif [ -e \"$backup\" ] || [ -L \"$backup\" ]; then rollback_failed=1; fi; fi; done < \"$installed_log\"\n"
        "      while IFS= read -r relative_path; do backup=\"$backup_root/$relative_path\"; destination=\"$install_path/$relative_path\"; if [ -e \"$backup\" ] || [ -L \"$backup\" ]; then if [ -e \"$destination\" ] || [ -L \"$destination\" ]; then current_identity=\"$(stat -c '%d:%i' -- \"$destination\" 2>/dev/null || true)\"; if [ \"$destination\" = \"$executable\" ] && [ \"$executable_replaced\" -eq 0 ] && [ \"$current_identity\" = \"$executable_original_identity\" ]; then :; else rollback_failed=1; fi; else mkdir -p \"$(dirname \"$destination\")\" || rollback_failed=1; if ! mv -nT -- \"$backup\" \"$destination\" || [ -e \"$backup\" ] || [ -L \"$backup\" ]; then rollback_failed=1; fi; fi; fi; done < \"$backup_log\"\n"
        "      if [ \"$rollback_failed\" -eq 0 ]; then rm -rf \"$transaction_path\"; fi\n"
        "      return 1\n"
        "    }\n"
        "    while IFS= read -r staged_file; do\n"
        "      relative_path=${staged_file#\"$new_root\"/}; destination=\"$install_path/$relative_path\"; backup=\"$backup_root/$relative_path\"\n"
        "      if ! mkdir -p \"$(dirname \"$destination\")\"; then rollback_zip; exit 1; fi\n"
        "      if [ -e \"$destination\" ] || [ -L \"$destination\" ]; then\n"
        "        if ! mkdir -p \"$(dirname \"$backup\")\"; then rollback_zip; exit 1; fi\n"
        "        if [ \"$destination\" = \"$executable\" ]; then\n"
        "          executable_original_identity=\"$(stat -c '%d:%i' -- \"$destination\")\" || { rollback_zip; exit 1; }\n"
        "          executable_backup=\"$backup\"\n"
        "          if ! cp -p \"$destination\" \"$backup\"; then rollback_zip; exit 1; fi\n"
        "          if ! printf '%s\\n' \"$relative_path\" >> \"$backup_log\"; then rollback_zip; exit 1; fi\n"
        "        else\n"
        "          if ! printf '%s\\n' \"$relative_path\" >> \"$backup_log\"; then rollback_zip; exit 1; fi\n"
        "          if ! mv \"$destination\" \"$backup\"; then rollback_zip; exit 1; fi\n"
        "        fi\n"
        "      fi\n"
        "      if [ \"$destination\" = \"$executable\" ]; then\n"
        "        replacement_temp=\"$destination.vita3k-update.$$\"\n"
        "        if ! cp -p \"$staged_file\" \"$replacement_temp\"; then rm -f \"$replacement_temp\" || true; rollback_zip; exit 1; fi\n"
        "        installed_identity=\"$(stat -c '%d:%i' -- \"$replacement_temp\")\" || { rm -f \"$replacement_temp\" || true; rollback_zip; exit 1; }\n"
        "        if ! mv -fT -- \"$replacement_temp\" \"$destination\"; then rm -f \"$replacement_temp\" || true; rollback_zip; exit 1; fi\n"
        "        executable_replaced=1; executable_identity=\"$installed_identity\"\n"
        "        if ! printf '%s\\n%s\\n' \"$relative_path\" \"$installed_identity\" >> \"$installed_log\"; then if [ \"$(stat -c '%d:%i' -- \"$destination\" 2>/dev/null || true)\" = \"$installed_identity\" ]; then rm -f \"$destination\" || true; fi; rollback_zip; exit 1; fi\n"
        "        else\n"
        "        replacement_temp=\"$destination.vita3k-update.$$\"\n"
        "        if ! cp -p \"$staged_file\" \"$replacement_temp\"; then rm -f \"$replacement_temp\" || true; rollback_zip; exit 1; fi\n"
        "        installed_identity=\"$(stat -c '%d:%i' -- \"$replacement_temp\")\" || { rm -f \"$replacement_temp\" || true; rollback_zip; exit 1; }\n"
        "        if ! mv -nT -- \"$replacement_temp\" \"$destination\" || [ -e \"$replacement_temp\" ] || [ -L \"$replacement_temp\" ]; then rm -f \"$replacement_temp\" || true; rollback_zip; exit 1; fi\n"
        "        if ! printf '%s\\n%s\\n' \"$relative_path\" \"$installed_identity\" >> \"$installed_log\"; then if [ \"$(stat -c '%d:%i' -- \"$destination\" 2>/dev/null || true)\" = \"$installed_identity\" ]; then rm -f \"$destination\" || true; fi; rollback_zip; exit 1; fi\n"
        "      fi\n"
        "    done < \"$transaction_path/files\"\n"
        "    ;;\n"
        "  appimage)\n"
        "    appimage_transaction=\"$(mktemp -d \"$(dirname \"$install_path\")/.vita3k-update.XXXXXX\")\"\n"
        "    appimage_staging=\"$appimage_transaction/new.AppImage\"; appimage_backup=\"$appimage_transaction/previous.AppImage\"\n"
        "    cp \"$payload\" \"$appimage_staging\"\n"
        "    chmod --reference=\"$install_path\" \"$appimage_staging\"\n"
        "    mv \"$install_path\" \"$appimage_backup\"\n"
        "    mv \"$appimage_staging\" \"$install_path\"\n"
        "    appimage_replaced=1\n"
        "    \"$install_path\" \"$@\" & appimage_pid=$!\n"
        "    if ! wait_for_startup \"$appimage_pid\"; then wait \"$appimage_pid\" || true; exit 1; fi\n"
        "    install_committed=1\n"
        "    rm -rf \"$appimage_transaction\" || true; appimage_transaction=; appimage_backup=; appimage_staging=; appimage_replaced=0; recovery_detail=\n"
        "    ;;\n"
        "  dmg)\n"
        "    mount_path=\"$(mktemp -d)\"\n"
        "    hdiutil attach -quiet -nobrowse -readonly -mountpoint \"$mount_path\" \"$payload\"\n"
        "    source_app=\"$(find \"$mount_path\" -maxdepth 2 -name '*.app' -print -quit)\"\n"
        "    if [ -z \"$source_app\" ]; then exit 1; fi\n"
        "    if [ -w \"$(dirname \"$install_path\")\" ]; then\n"
        "      transaction_dir=\"$(mktemp -d \"$(dirname \"$install_path\")/.vita3k-update.XXXXXX\")\"\n"
        "      new_app=\"$transaction_dir/new.app\"; backup_app=\"$transaction_dir/backup.app\"\n"
        "      ditto \"$source_app\" \"$new_app\"\n"
        "      mv \"$install_path\" \"$backup_app\"\n"
        "      dmg_backup=\"$backup_app\"; dmg_replaced=1\n"
        "      mv \"$new_app\" \"$install_path\"\n"
        "    else\n"
        "      admin_output=\"$(/usr/bin/osascript -e 'on run argv' -e 'set output to do shell script \"transaction_dir=$(/usr/bin/mktemp -d \" & quoted form of (item 1 of argv) & \"/.vita3k-update.XXXXXX) && if /usr/bin/ditto \" & quoted form of (item 2 of argv) & \" \\\"$transaction_dir/new.app\\\" && /bin/mv \" & quoted form of (item 3 of argv) & \" \\\"$transaction_dir/backup.app\\\" ; then if /bin/mv \\\"$transaction_dir/new.app\\\" \" & quoted form of (item 3 of argv) & \"; then printf \\\"TRANSACTION_DIR=%s\\\\n\\\" \\\"$transaction_dir\\\"; else if /bin/mv \\\"$transaction_dir/backup.app\\\" \" & quoted form of (item 3 of argv) & \"; then /bin/rm -rf \\\"$transaction_dir\\\"; else printf \\\"RECOVERY_DIR=%s\\\\n\\\" \\\"$transaction_dir\\\"; fi; fi; else if /bin/rm -rf \\\"$transaction_dir\\\"; then printf \\\"INSTALL_FAILED\\\\n\\\"; else printf \\\"RECOVERY_DIR=%s\\\\n\\\" \\\"$transaction_dir\\\"; fi; fi\" with administrator privileges' -e 'return output' -e 'end run' \"$(dirname \"$install_path\")\" \"$source_app\" \"$install_path\" 2>&1)\" || { admin_status=$?; case \"$admin_output\" in *RECOVERY_DIR=*) rollback_failed=1; recovery_detail=\"$admin_output\";; esac; exit \"$admin_status\"; }\n"
        "      case \"$admin_output\" in *TRANSACTION_DIR=*) transaction_dir=\"$(printf '%s\\n' \"$admin_output\" | sed -n 's/^TRANSACTION_DIR=//p' | head -n 1)\"; dmg_backup=\"$transaction_dir/backup.app\"; dmg_replaced=1; dmg_elevated=1 ;; *RECOVERY_DIR=*) rollback_failed=1; recovery_detail=\"$admin_output\"; exit 1 ;; *INSTALL_FAILED*) exit 1 ;; *) exit 1 ;; esac\n"
        "    fi\n"
        "    \"$executable\" \"$@\" & restart_pid=$!\n"
        "    if ! wait_for_startup \"$restart_pid\"; then wait \"$restart_pid\" || true; exit 1; fi\n"
        "    install_committed=1\n"
        "    if [ -n \"$transaction_dir\" ]; then if remove_dmg_transaction; then transaction_dir=; dmg_backup=; dmg_replaced=0; dmg_elevated=0; else show_update_error \"Vita3K started, but the update backup remains at: $transaction_dir\"; fi; fi\n"
        "    ;;\n"
        "esac\n"
        "if [ \"$mode\" = zip ]; then\n"
        "  \"$executable\" \"$@\" & restart_pid=$!\n"
        "  if ! wait_for_startup \"$restart_pid\"; then wait \"$restart_pid\" || true; rollback_zip || true; exit 1; fi\n"
        "  install_committed=1\n"
        "  rm -rf \"$transaction_path\"; transaction_path=\n"
        "fi\n"
        "cleanup_mount\n"
        "rm -rf \"$temp_path\"\n");
    const QByteArray script_contents_utf8 = script_contents.toUtf8();
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || script.write(script_contents_utf8) != script_contents_utf8.size()
        || !script.commit()) {
        QMessageBox::warning(m_parent_widget, tr_update("Restart Failed"), tr_update("The update installer could not be started."));
        release_prepared_update();
        return;
    }
    QFile::setPermissions(script_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);

    helper_program = QStringLiteral("/bin/sh");
    helper_arguments = {
        script_path,
        process_id,
        m_update_mode,
        m_update_mode == QStringLiteral("zip") ? m_stage_path : m_download_path,
        m_install_path,
        restart_executable,
        m_update_directory->path(),
        cancel_path,
        approval_path,
    };
    helper_arguments.append(arguments);
#endif

    m_update_directory->setAutoRemove(false);
    if (!QProcess::startDetached(helper_program, helper_arguments, m_original_working_directory)) {
        m_update_directory->setAutoRemove(true);
        delete m_update_directory;
        m_update_directory = nullptr;
        m_stage_path.clear();
        m_update_mode.clear();
        QMessageBox::warning(m_parent_widget, tr_update("Restart Failed"), tr_update("The update installer could not be started."));
        return;
    }

    if (m_parent_widget && !m_parent_widget->close()) {
        if (!QFile::rename(cancel_placeholder_path, cancel_path)) {
            QFile cancel_marker(cancel_path);
            if (cancel_marker.open(QIODevice::WriteOnly))
                cancel_marker.close();
        }
        m_update_directory->setAutoRemove(false);
        delete m_update_directory;
        m_update_directory = nullptr;
        m_stage_path.clear();
        m_update_mode.clear();
        QMessageBox::information(m_parent_widget, tr_update("Update Canceled"), tr_update("Vita3K stayed open, so the update was canceled. Try again when you're ready to restart."));
        return;
    }

    QSaveFile approval_marker(approval_path);
    if (!approval_marker.open(QIODevice::WriteOnly)
        || approval_marker.write("approved", 8) != 8
        || !approval_marker.commit()) {
        QFile cancel_marker(cancel_path);
        if (cancel_marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
            cancel_marker.close();
        const bool relaunched = QProcess::startDetached(restart_executable, arguments, m_original_working_directory);
        QString restart_error = tr_update("The update shutdown approval could not be saved, so the update was not applied.");
        if (!relaunched)
            restart_error += QStringLiteral("\n\n") + tr_update("Vita3K could not be reopened.");
        QMessageBox::warning(nullptr, tr_update("Restart Failed"), restart_error);
        if (m_update_directory) {
            m_update_directory->setAutoRemove(false);
            delete m_update_directory;
            m_update_directory = nullptr;
        }
        m_stage_path.clear();
        m_update_mode.clear();
        return;
    }

    if (!m_parent_widget)
        QCoreApplication::quit();
}
