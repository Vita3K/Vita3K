#pragma once

#include <updater/state.h>

#include <QObject>
#include <QPointer>
#include <QThread>

#include <optional>

class QProgressDialog;
class QWidget;

class UpdateManager final : public QObject {
    Q_OBJECT

public:
    explicit UpdateManager(QObject *parent = nullptr);
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

    QPointer<QWidget> m_parent_widget;
    QPointer<QProgressDialog> m_progress_dialog;
    QThread *m_worker_thread = nullptr;
    std::optional<updater::UpdateCheckResult> m_pending_update;
    bool m_update_available = false;
};
