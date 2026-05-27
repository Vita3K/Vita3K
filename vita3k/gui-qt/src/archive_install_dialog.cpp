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

#include <gui-qt/archive_install_dialog.h>
#include <gui-qt/qt_utils.h>

#include <util/string_utils.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QThread>
#include <QVBoxLayout>

#include <algorithm>
#include <set>

namespace {

std::vector<fs::path> archives_in_dir(const fs::path &dir) {
    std::vector<fs::path> result;
    for (const auto &entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file())
            continue;
        const auto ext = string_utils::tolower(entry.path().extension().string());
        if (ext == ".zip" || ext == ".vpk")
            result.push_back(entry.path());
    }
    return result;
}

QFrame *make_section_frame(QWidget *parent) {
    auto *frame = new QFrame(parent);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return frame;
}

QLabel *make_section_title(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

QLabel *make_muted_label(const QString &text, QWidget *parent, Qt::Alignment alignment = Qt::AlignLeft) {
    auto *label = new QLabel(text, parent);
    label->setAlignment(alignment);
    label->setWordWrap(true);
    label->setProperty("themeRole", QStringLiteral("mutedText"));
    return label;
}

QFrame *make_result_row(const ArchiveInstallResult &result, QWidget *parent) {
    auto *row = new QFrame(parent);
    row->setFrameShape(QFrame::StyledPanel);

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);

    auto *text_col = new QVBoxLayout();
    text_col->setContentsMargins(0, 0, 0, 0);
    text_col->setSpacing(2);

    auto *title = new QLabel(result.title.isEmpty() ? QObject::tr("Unknown Content") : result.title, row);
    title->setWordWrap(true);
    QFont title_font = title->font();
    title_font.setBold(true);
    title->setFont(title_font);
    text_col->addWidget(title);

    QStringList meta_parts;
    if (!result.title_id.isEmpty())
        meta_parts.push_back(result.title_id);
    text_col->addWidget(make_muted_label(meta_parts.join(QStringLiteral(" | ")), row));

    layout->addLayout(text_col, 1);

    auto *status = new QLabel(result.success ? QObject::tr("Installed") : QObject::tr("Failed"), row);
    status->setAlignment(Qt::AlignCenter);
    status->setMinimumWidth(84);
    status->setMargin(6);
    status->setProperty("severity", result.success ? QStringLiteral("success") : QStringLiteral("error"));
    layout->addWidget(status, 0, Qt::AlignTop);

    return row;
}

} // namespace

void ArchiveWorker::run() {
    const int total = static_cast<int>(m_archives.size());
    int done = 0;

    for (const auto &archive_path : m_archives) {
        const QString archive_name = gui::utils::to_qt_path(archive_path.filename());
        Q_EMIT current_title_changed(archive_name);
        Q_EMIT global_progress(done + 1, total);

        const auto progress_cb = [this](const ArchiveContents &v) {
            if (v.count.has_value() && v.current.has_value())
                Q_EMIT content_progress(
                    static_cast<int>(*v.current),
                    static_cast<int>(*v.count));
            if (v.progress.has_value())
                Q_EMIT file_progress(*v.progress);
        };

        const std::vector<ContentInfo> contents = install_archive(m_emuenv, archive_path, progress_cb, m_reinstall_cb);

        if (contents.empty()) {
            ArchiveInstallResult r;
            r.archive_name = archive_name;
            r.archive_full_path = archive_path;
            r.title = QObject::tr("(incompatible or no content found)");
            r.success = false;
            m_results.append(r);
        } else {
            for (const auto &info : contents) {
                ArchiveInstallResult r;
                r.archive_name = archive_name;
                r.archive_full_path = archive_path;
                r.title = QString::fromStdString(info.title);
                r.title_id = QString::fromStdString(info.title_id);
                r.category = QString::fromStdString(info.category);
                r.content_id = QString::fromStdString(info.content_id);
                r.success = info.state;
                m_results.append(r);

                Q_EMIT current_title_changed(r.title);
            }
        }

        ++done;
        Q_EMIT global_progress(done, total);
    }

    Q_EMIT finished(true);
}

ArchiveInstallDialog::ArchiveInstallDialog(EmuEnvState &emuenv, QWidget *parent)
    : QDialog(parent)
    , m_emuenv(emuenv) {
    setWindowTitle(tr("Install Archive"));
    setModal(true);

    const std::vector<fs::path> archives = pick_archives();
    if (archives.empty()) {
        QMetaObject::invokeMethod(this, &QDialog::reject, Qt::QueuedConnection);
        return;
    }

    run_install(archives);
}

ArchiveInstallDialog::ArchiveInstallDialog(EmuEnvState &emuenv, const std::vector<fs::path> &archives, QWidget *parent)
    : QDialog(parent)
    , m_emuenv(emuenv) {
    setWindowTitle(tr("Install Archive"));
    setModal(true);

    if (archives.empty()) {
        QMetaObject::invokeMethod(this, &QDialog::reject, Qt::QueuedConnection);
        return;
    }

    run_install(archives);
}

std::vector<fs::path> ArchiveInstallDialog::pick_archives() {
    QDialog type_picker(this);
    type_picker.setWindowTitle(tr("Select Install Type"));
    type_picker.setModal(true);

    auto *layout = new QVBoxLayout(&type_picker);
    layout->addWidget(new QLabel(tr("What would you like to install?"), &type_picker));

    auto *btn_file = new QPushButton(tr("Select a file (.zip / .vpk)..."), &type_picker);
    auto *btn_dir = new QPushButton(tr("Select a directory (installs all archives inside)..."), &type_picker);
    auto *btn_cancel = new QPushButton(tr("Cancel"), &type_picker);
    layout->addWidget(btn_file);
    layout->addWidget(btn_dir);
    layout->addWidget(btn_cancel);

    enum class Choice { None,
        File,
        Dir };
    Choice choice = Choice::None;

    connect(btn_file, &QPushButton::clicked, &type_picker, [&] {
        choice = Choice::File;
        type_picker.accept();
    });
    connect(btn_dir, &QPushButton::clicked, &type_picker, [&] {
        choice = Choice::Dir;
        type_picker.accept();
    });
    connect(btn_cancel, &QPushButton::clicked, &type_picker, &QDialog::reject);

    if (type_picker.exec() != QDialog::Accepted || choice == Choice::None)
        return {};

    if (choice == Choice::File) {
        const QString path = QFileDialog::getOpenFileName(
            this,
            tr("Select Archive"),
            QString(),
            tr("PlayStation Vita commercial software package (NoNpDrm/FAGDec) / PlayStation Vita homebrew software package (*.zip *.vpk);;PlayStation Vita commercial software package (NoNpDrm/FAGDec) (*.zip);;PlayStation Vita homebrew software package (*.vpk)"));
        if (path.isEmpty())
            return {};
        return { gui::utils::to_fs_path(path) };
    }

    const QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Directory Containing Archives"));
    if (dir.isEmpty())
        return {};

    auto archives = archives_in_dir(gui::utils::to_fs_path(dir));
    if (archives.empty()) {
        QMessageBox::information(this, tr("No Archives Found"),
            tr("The selected directory contains no .zip or .vpk files."));
        return {};
    }
    return archives;
}
void ArchiveInstallDialog::run_install(const std::vector<fs::path> &archives) {
    const int total_archives = static_cast<int>(archives.size());

    auto *root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(16, 16, 16, 16);
    root_layout->setSpacing(12);

    auto *title_label = new QLabel(tr("Installing Archive"), this);
    QFont title_font = title_label->font();
    title_font.setBold(true);
    title_font.setPointSize(title_font.pointSize() + 1);
    title_label->setFont(title_font);
    root_layout->addWidget(title_label);

    auto *app_label = make_muted_label(tr("Preparing installation..."), this);
    app_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    root_layout->addWidget(app_label);

    auto *progress_frame = make_section_frame(this);
    auto *progress_layout = new QGridLayout(progress_frame);
    progress_layout->setContentsMargins(14, 14, 14, 14);
    progress_layout->setHorizontalSpacing(12);
    progress_layout->setVerticalSpacing(10);

    auto *global_title = make_section_title(tr("Archive Progress"), progress_frame);
    auto *global_label = make_muted_label(tr("Archive 1 of %1").arg(total_archives), progress_frame, Qt::AlignRight | Qt::AlignVCenter);
    auto *global_bar = new QProgressBar(progress_frame);
    global_bar->setRange(0, total_archives);
    global_bar->setValue(0);

    auto *content_title = make_section_title(tr("Content Progress"), progress_frame);
    auto *content_label = make_muted_label(tr("Content 0 of 0"), progress_frame, Qt::AlignRight | Qt::AlignVCenter);
    auto *content_bar = new QProgressBar(progress_frame);
    content_bar->setRange(0, 1);
    content_bar->setValue(0);

    auto *file_title = make_section_title(tr("File Extraction"), progress_frame);
    auto *file_label = make_muted_label(tr("0% complete"), progress_frame, Qt::AlignRight | Qt::AlignVCenter);
    auto *file_bar = new QProgressBar(progress_frame);
    file_bar->setRange(0, 100);
    file_bar->setValue(0);

    progress_layout->addWidget(global_title, 0, 0);
    progress_layout->addWidget(global_label, 0, 1);
    progress_layout->addWidget(global_bar, 1, 0, 1, 2);
    progress_layout->addWidget(content_title, 2, 0);
    progress_layout->addWidget(content_label, 2, 1);
    progress_layout->addWidget(content_bar, 3, 0, 1, 2);
    progress_layout->addWidget(file_title, 4, 0);
    progress_layout->addWidget(file_label, 4, 1);
    progress_layout->addWidget(file_bar, 5, 0, 1, 2);

    root_layout->addWidget(progress_frame);

    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    setMinimumWidth(520);

    const ReinstallCallback reinstall_cb = [this](const std::string &title, const std::string &title_id) -> bool {
        bool result = false;
        const QString title_qt = QString::fromStdString(title);
        const QString title_id_qt = QString::fromStdString(title_id);
        const QString label = title_qt.isEmpty()
            ? title_id_qt
            : tr("%1 [%2]").arg(title_qt, title_id_qt);
        QMetaObject::invokeMethod(
            this, [this, &result, label]() { result = (QMessageBox::question(
                                                           this,
                                                           tr("Already Installed"),
                                                           tr("%1 is already installed.\nDo you want to overwrite it?").arg(label),
                                                           QMessageBox::Yes | QMessageBox::No,
                                                           QMessageBox::No)
                                                 == QMessageBox::Yes); }, Qt::BlockingQueuedConnection);
        return result;
    };

    auto *thread = new QThread(this);
    auto *worker = new ArchiveWorker(m_emuenv, archives, reinstall_cb);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &ArchiveWorker::run);

    connect(worker, &ArchiveWorker::global_progress, this,
        [global_bar, global_label, total_archives](int current, int) {
            global_bar->setValue(current);
            global_label->setText(QObject::tr("Archive %1 of %2").arg(current).arg(total_archives));
        });

    connect(worker, &ArchiveWorker::content_progress, this,
        [content_bar, content_label](int done, int total) {
            content_bar->setRange(0, total);
            content_bar->setValue(done);
            content_label->setText(QObject::tr("Content %1 of %2").arg(done).arg(total));
        });

    connect(worker, &ArchiveWorker::file_progress, this,
        [file_bar, file_label](float pct) {
            const int v = static_cast<int>(pct);
            file_bar->setValue(v);
            file_label->setText(QObject::tr("%1% complete").arg(v));
        });

    connect(worker, &ArchiveWorker::current_title_changed, this,
        [app_label](const QString &title) {
            app_label->setText(title);
        });
    connect(worker, &ArchiveWorker::finished, this,
        [this, worker, thread](bool) {
            thread->quit();
            thread->wait();

            const QList<ArchiveInstallResult> results = worker->results();
            worker->deleteLater();

            setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);
            show();

            QLayoutItem *item;
            while ((item = layout()->takeAt(0)) != nullptr) {
                delete item->widget();
                delete item;
            }

            auto *summary = new QLabel(this);
            const int success_count = std::count_if(results.begin(), results.end(), [](const ArchiveInstallResult &r) { return r.success; });
            const int failure_count = results.size() - success_count;
            summary->setText(
                (failure_count == 0)
                    ? tr("Installation complete. %1 item(s) installed successfully.").arg(success_count)
                    : tr("Installation complete. %1 succeeded, %2 failed.").arg(success_count).arg(failure_count));
            summary->setWordWrap(true);
            layout()->addWidget(summary);

            auto *scroll = new QScrollArea(this);
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            auto *inner = new QWidget(scroll);
            auto *inner_layout = new QVBoxLayout(inner);
            inner_layout->setContentsMargins(0, 0, 0, 0);
            inner_layout->setSpacing(10);

            QMap<QString, QList<const ArchiveInstallResult *>> by_archive;
            for (const auto &r : results)
                by_archive[r.archive_name].append(&r);

            for (auto it = by_archive.constBegin(); it != by_archive.constEnd(); ++it) {
                auto *archive_frame = make_section_frame(inner);
                auto *archive_layout = new QVBoxLayout(archive_frame);
                archive_layout->setContentsMargins(14, 14, 14, 14);
                archive_layout->setSpacing(10);

                auto *archive_title = make_section_title(it.key(), archive_frame);
                archive_title->setWordWrap(true);
                archive_layout->addWidget(archive_title);

                for (const auto *r : it.value()) {
                    archive_layout->addWidget(make_result_row(*r, archive_frame));
                }

                inner_layout->addWidget(archive_frame);
            }
            inner_layout->addStretch();

            inner->setLayout(inner_layout);
            scroll->setWidget(inner);
            scroll->setMinimumHeight(170);
            scroll->setMaximumHeight(320);
            layout()->addWidget(scroll);

            auto *del_check = new QCheckBox(tr("Delete archive files after install?"), this);
            layout()->addWidget(del_check);

            auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
            buttons->setCenterButtons(false);
            layout()->addWidget(buttons);

            connect(buttons, &QDialogButtonBox::accepted, this,
                [this, del_check, results]() {
                    if (del_check->isChecked()) {
                        std::set<fs::path> deleted;
                        QStringList failed;
                        for (const auto &r : results) {
                            if (r.archive_full_path.empty() || !deleted.insert(r.archive_full_path).second)
                                continue;
                            // fs::remove throws and crashes the app when the archive is locked so use the non throwing overload
                            boost::system::error_code error;
                            fs::remove(r.archive_full_path, error);
                            if (error)
                                failed.push_back(r.archive_name);
                        }

                        if (!failed.isEmpty()) {
                            QMessageBox::warning(this, tr("Could Not Delete Archives"),
                                tr("The following archive files could not be deleted. They may still be open in another program:\n\n%1").arg(failed.join(QStringLiteral("\n"))));
                        }
                    }
                    Q_EMIT install_complete(results);
                    accept();
                });

            resize(560, std::clamp(250 + (static_cast<int>(results.size()) * 56), 320, 520));
            show();
        });

    thread->start();
}
