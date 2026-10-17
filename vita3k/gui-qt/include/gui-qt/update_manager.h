#pragma once

#include <updater/state.h>

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QThread>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

class QNetworkReply;
class QProgressDialog;
class QWidget;

class UpdateManager final : public QObject {
    Q_OBJECT

public:
    explicit UpdateManager(QObject *parent = nullptr);
    ~UpdateManager() override;

    void check_for_updates(updater::UpdateCheckMode mode, QWidget *parent = nullptr);
    void review_pending_update(QWidget *parent = nullptr);
    void set_skipped_build(std::uint64_t build_number) { m_skipped_build = build_number; }
    void set_restart_arguments(QStringList arguments) { m_restart_arguments = std::move(arguments); }

Q_SIGNALS:
    void update_available_changed(bool available);
    void skip_build_requested(std::uint64_t build_number);

private:
    void set_update_available(bool available);
    void set_pending_update(std::optional<updater::UpdateCheckResult> result);
    void start_worker(const std::function<void()> &task);
    void close_progress_dialog();
    void close_download_dialog();
    void handle_check_result(updater::UpdateCheckMode mode, const updater::UpdateCheckResult &result);
    void show_update_message(const updater::UpdateCheckResult &result, bool allow_skip);
    void start_update_download(const updater::UpdateInfo &info);
    void on_download_progress(qint64 received, qint64 total);
    void on_download_finished();
    void finish_update(bool installed, const QString &error_message);
    void prompt_restart();

    QPointer<QWidget> m_parent_widget;
    QPointer<QProgressDialog> m_progress_dialog;
    QPointer<QProgressDialog> m_download_dialog;
    QPointer<QNetworkReply> m_download_reply;
    QNetworkAccessManager m_network;
    QThread *m_worker_thread = nullptr;
    std::optional<updater::UpdateCheckResult> m_pending_update;
    bool m_update_available = false;
    std::uint64_t m_skipped_build = 0;
    QString m_expected_sha256;
    QStringList m_restart_arguments;
    bool m_restart_pending = false;
};
