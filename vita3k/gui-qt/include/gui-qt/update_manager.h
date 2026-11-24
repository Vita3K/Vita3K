#pragma once

#include <gui-qt/gui_settings.h>
#include <updater/state.h>

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QThread>

#include <memory>
#include <optional>

class QProgressDialog;
class QNetworkReply;
class QFile;
class QTemporaryDir;
class QUrl;
class QWidget;

class UpdateManager final : public QObject {
    Q_OBJECT

public:
    explicit UpdateManager(std::shared_ptr<GuiSettings> gui_settings, QObject *parent = nullptr);
    ~UpdateManager() override;

    void check_for_updates(updater::UpdateCheckMode mode, QWidget *parent = nullptr);
    void review_pending_update(QWidget *parent = nullptr);

Q_SIGNALS:
    void update_available_changed(bool available);

private:
    void set_update_available(bool available);
    void set_pending_update(std::optional<updater::UpdateCheckResult> result);
    void start_worker(const std::function<void()> &task);
    void close_progress_dialog();
    void handle_check_result(updater::UpdateCheckMode mode, const updater::UpdateCheckResult &result);
    void show_update_message(const updater::UpdateCheckResult &result);
    void download_update(const updater::UpdateAsset &asset);
    void start_download_request(const QUrl &url);
    void receive_download_data();
    void update_download_progress(qint64 received, qint64 total);
    void finish_download();
    bool prepare_update(QString &error_message);
    void show_update_complete();
    void restart_with_update();

    QPointer<QWidget> m_parent_widget;
    std::shared_ptr<GuiSettings> m_gui_settings;
    QPointer<QProgressDialog> m_progress_dialog;
    QPointer<QNetworkReply> m_download_reply;
    QNetworkAccessManager m_network;
    QFile *m_download_file = nullptr;
    QTemporaryDir *m_update_directory = nullptr;
    QString m_download_path;
    QString m_stage_path;
    QString m_asset_name;
    qint64 m_expected_download_size = 0;
    int m_download_redirect_count = 0;
    QString m_update_mode;
    QString m_install_path;
    QString m_download_save_error;
    QThread *m_worker_thread = nullptr;
    std::optional<updater::UpdateCheckResult> m_pending_update;
    bool m_update_available = false;
    bool m_download_canceled = false;
};
