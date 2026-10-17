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

#include <gui-qt/save_data_manager_dialog.h>

#include <gui-qt/gui_settings.h>
#include <gui-qt/qt_utils.h>

#include <app/functions.h>
#include <io/state.h>
#include <packages/savedata.h>

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <map>

static constexpr int RoleSort = Qt::UserRole + 1;
static constexpr int RoleOriginalPixmap = Qt::UserRole + 2;
static constexpr int RoleDetails = Qt::UserRole + 3;
static constexpr int RoleAppTitle = Qt::UserRole + 4;
static constexpr int RoleSavePath = Qt::UserRole + 5;

static const QString notes_section = QStringLiteral("SaveDataNotes");
static const GuiSave sm_geometry = GuiSave(QStringLiteral("SaveDataManager"), QStringLiteral("geometry"), QByteArray());
static const GuiSave sm_headerState = GuiSave(QStringLiteral("SaveDataManager"), QStringLiteral("headerState"), QByteArray());
static const GuiSave sm_iconSize = GuiSave(QStringLiteral("SaveDataManager"), QStringLiteral("iconSize"), 30);

namespace {

QString format_date_time(const SceDateTime &t) {
    return QString::asprintf("%04u-%02u-%02u %02u:%02u:%02u", t.year, t.month, t.day, t.hour, t.minute, t.second);
}

uint64_t date_time_key(const SceDateTime &t) {
    return ((((static_cast<uint64_t>(t.year) * 13 + t.month) * 32 + t.day) * 24 + t.hour) * 60 + t.minute) * 60 + t.second;
}

QDateTime last_modified(const fs::path &dir) {
    std::time_t newest = 0;
    boost::system::error_code ec;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (fs::is_regular_file(it->status()))
            newest = std::max(newest, fs::last_write_time(it->path(), ec));
    }
    return QDateTime::fromSecsSinceEpoch(newest);
}

QString escape(const std::string &s) {
    return QString::fromStdString(s).toHtmlEscaped();
}

bool is_same_or_inside(const fs::path &path, const fs::path &base) {
    const auto canonical = [](const fs::path &p) {
        boost::system::error_code ec;
        const fs::path result = fs::weakly_canonical(p, ec);
        return ec ? fs::absolute(p) : result;
    };
    const fs::path p = canonical(path);
    const fs::path b = canonical(base);
    return std::mismatch(b.begin(), b.end(), p.begin(), p.end()).first == b.end();
}

} // namespace

SaveDataManagerDialog::SaveDataManagerDialog(EmuEnvState &emuenv, std::shared_ptr<GuiSettings> gui_settings,
    std::function<bool()> is_app_running, QWidget *parent)
    : QDialog(parent, Qt::Window)
    , m_emuenv(emuenv)
    , m_gui_settings(std::move(gui_settings))
    , m_is_app_running(std::move(is_app_running)) {
    setWindowTitle(tr("Save Data Manager"));
    setObjectName(QStringLiteral("save_data_manager_dialog"));
    setWindowFlag(Qt::WindowMinMaxButtonsHint, true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setAttribute(Qt::WA_DeleteOnClose);

    m_model = new QStandardItemModel(0, ColumnCount, this);
    m_model->setHorizontalHeaderLabels({ tr("Icon"), tr("Title & Subtitle"), tr("Last Modified"), tr("Save ID"), tr("Notes") });

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(RoleSort);
    m_proxy->setFilterKeyColumn(-1);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_table->setSortingEnabled(true);
    m_table->setShowGrid(false);
    m_table->setWordWrap(true);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(Icon, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(Title, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(Modified, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(SaveId, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(Notes, QHeaderView::Interactive);
    m_table->sortByColumn(Title, Qt::AscendingOrder);

    m_detail_icon = new QLabel(this);
    m_detail_icon->setAlignment(Qt::AlignCenter);
    m_detail_icon->setMinimumHeight(160);
    m_detail_text = new QLabel(this);
    m_detail_text->setWordWrap(true);
    m_detail_text->setTextFormat(Qt::RichText);
    m_detail_text->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_detail_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *detail_scroll = new QScrollArea(this);
    detail_scroll->setWidget(m_detail_text);
    detail_scroll->setWidgetResizable(true);
    detail_scroll->setFrameShape(QFrame::NoFrame);

    auto *import_button = new QPushButton(tr("Import from PS Vita (Decrypt)..."), this);
    import_button->setToolTip(tr("Decrypts a savedata folder copied from ux0:user/00/savedata on a PS Vita and adds it here."));
    m_export_button = new QPushButton(tr("Export for PS Vita (Encrypt)..."), this);
    m_export_button->setToolTip(tr("Encrypts the selected savedata so it can be copied back to ux0:user/00/savedata on a PS Vita."));
    m_delete_button = new QPushButton(tr("Delete Selection"), this);
    m_open_button = new QPushButton(tr("View Folder"), this);

    auto *detail_layout = new QVBoxLayout;
    detail_layout->addWidget(m_detail_icon);
    detail_layout->addWidget(detail_scroll, 1);
    detail_layout->addWidget(import_button);
    detail_layout->addWidget(m_export_button);
    detail_layout->addSpacing(8);
    detail_layout->addWidget(m_delete_button);
    detail_layout->addWidget(m_open_button);
    auto *detail_widget = new QWidget(this);
    detail_widget->setLayout(detail_layout);
    detail_widget->setFixedWidth(360);

    auto *top = new QHBoxLayout;
    top->addWidget(m_table, 1);
    top->addWidget(detail_widget);

    m_search = new QLineEdit(this);
    m_search->setClearButtonEnabled(true);
    m_icon_slider = new QSlider(Qt::Horizontal, this);
    m_icon_slider->setRange(0, 100);
    m_icon_slider->setFixedWidth(240);
    auto *close_button = new QPushButton(tr("Close"), this);

    auto *bottom = new QHBoxLayout;
    bottom->addWidget(new QLabel(tr("Search:"), this));
    bottom->addWidget(m_search, 1);
    bottom->addSpacing(12);
    bottom->addWidget(new QLabel(tr("Icon size:"), this));
    bottom->addWidget(m_icon_slider);
    bottom->addStretch();
    bottom->addWidget(close_button);

    auto *root = new QVBoxLayout(this);
    root->addLayout(top, 1);
    root->addLayout(bottom);

    connect(m_search, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);
    connect(m_icon_slider, &QSlider::valueChanged, this, &SaveDataManagerDialog::update_icon_size);
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &SaveDataManagerDialog::update_details);
    connect(m_model, &QStandardItemModel::itemChanged, this, [this](QStandardItem *item) {
        if (item->column() != Notes || item->data(RoleSort).toString() == item->text())
            return;
        const QString save_id = m_model->item(item->row(), SaveId)->text();
        m_gui_settings->set_value(notes_section, save_id, item->text());
        item->setData(item->text(), RoleSort);
        update_details();
    });
    connect(import_button, &QPushButton::clicked, this, &SaveDataManagerDialog::import_from_vita);
    connect(m_export_button, &QPushButton::clicked, this, &SaveDataManagerDialog::export_for_vita);
    connect(m_delete_button, &QPushButton::clicked, this, &SaveDataManagerDialog::delete_selected);
    connect(m_open_button, &QPushButton::clicked, this, [this] {
        if (const fs::path path = selected_save_path(); !path.empty())
            gui::utils::open_dir(path);
    });
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);

    resize(1280, 720);
    restoreGeometry(m_gui_settings->get_value(sm_geometry).toByteArray());
    m_icon_slider->setValue(m_gui_settings->get_value(sm_iconSize).toInt());
    update_icon_size(m_icon_slider->value());

    reload();
    m_table->horizontalHeader()->restoreState(m_gui_settings->get_value(sm_headerState).toByteArray());
}

SaveDataManagerDialog::~SaveDataManagerDialog() {
    m_gui_settings->set_value(sm_geometry, saveGeometry(), false);
    m_gui_settings->set_value(sm_headerState, m_table->horizontalHeader()->saveState(), false);
    m_gui_settings->set_value(sm_iconSize, m_icon_slider->value());
}

fs::path SaveDataManagerDialog::savedata_root() const {
    return m_emuenv.vita_fs_path / "ux0/user" / m_emuenv.io.user_id / "savedata";
}

QString SaveDataManagerDialog::selected_save_id() const {
    const QModelIndex current = m_table->currentIndex();
    if (!current.isValid())
        return {};
    return m_proxy->index(current.row(), SaveId).data().toString();
}

fs::path SaveDataManagerDialog::selected_save_path() const {
    const QModelIndex current = m_table->currentIndex();
    if (!current.isValid())
        return {};
    return gui::utils::to_fs_path(m_proxy->index(current.row(), SaveId).data(RoleSavePath).toString());
}

void SaveDataManagerDialog::reload() {
    const QString selected = selected_save_id();

    std::map<std::string, app::AppEntry> apps;
    for (auto &app : app::get_apps(m_emuenv))
        apps.emplace(app.savedata, app);

    m_model->removeRows(0, m_model->rowCount());

    boost::system::error_code ec;
    for (fs::directory_iterator it(savedata_root(), ec), end; !ec && it != end; it.increment(ec)) {
        if (!fs::is_directory(it->status()))
            continue;
        const fs::path path = it->path();
        const std::string save_id = path.filename().string();
        if (save_id.starts_with('.') || path.extension() == ".tmp" || path.extension() == ".bak")
            continue;
        if (save_id.starts_with("NPXS") && save_id != "NPXS10007")
            continue;

        const auto app = apps.find(save_id);
        const std::string app_title = app != apps.end() ? app->second.title : save_id;
        auto slot_list = savedata::read_slots(path);
        std::sort(slot_list.begin(), slot_list.end(), [](const auto &a, const auto &b) { return date_time_key(a.modified) > date_time_key(b.modified); });

        QString title = QString::fromStdString(app_title);
        if (!slot_list.empty())
            title += QStringLiteral("\n") + QString::fromStdString(slot_list.front().title + " " + slot_list.front().subtitle).trimmed();

        const bool encrypted = savedata::is_encrypted(path);
        const bool exportable = savedata::can_export(path);
        QString details = QStringLiteral("<b>%1</b><br>").arg(escape(app_title));
        details += tr("Save ID: %1").arg(QString::fromStdString(save_id)) + QStringLiteral("<br>");
        QString export_state = tr("unavailable, not created on a PS Vita");
        if (encrypted)
            export_state = tr("still encrypted, import it first");
        else if (exportable)
            export_state = tr("available");
        details += tr("PS Vita export: %1").arg(export_state) + QStringLiteral("<br>");
        for (const auto &entry : slot_list) {
            details += QStringLiteral("<br><b>") + tr("Slot %1").arg(entry.id) + QStringLiteral("</b> ") + format_date_time(entry.modified).toHtmlEscaped();
            details += QStringLiteral("<br>") + escape(entry.title);
            if (!entry.subtitle.empty())
                details += QStringLiteral("<br>") + escape(entry.subtitle);
            if (!entry.detail.empty())
                details += QStringLiteral("<br><small>") + escape(entry.detail).replace('\n', QStringLiteral("<br>")) + QStringLiteral("</small>");
        }

        // Slot times survive importing and exporting, unlike file times
        QDateTime modified = last_modified(path);
        if (!slot_list.empty()) {
            const SceDateTime &t = slot_list.front().modified;
            const QDateTime slot_time(QDate(t.year, t.month, t.day), QTime(t.hour, t.minute, t.second));
            if (slot_time.isValid())
                modified = slot_time;
        }
        const QString note = m_gui_settings->get_value(notes_section, QString::fromStdString(save_id), QString()).toString();

        auto *icon_item = new QStandardItem;
        icon_item->setEditable(false);
        icon_item->setData(QString::fromStdString(app_title), RoleSort);
        icon_item->setData(details, RoleDetails);
        icon_item->setData(QString::fromStdString(app_title), RoleAppTitle);
        auto *title_item = new QStandardItem(title);
        title_item->setEditable(false);
        title_item->setData(title.toLower(), RoleSort);
        auto *modified_item = new QStandardItem(modified.toSecsSinceEpoch() > 0 ? modified.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("-"));
        modified_item->setEditable(false);
        modified_item->setData(modified, RoleSort);
        auto *id_item = new QStandardItem(QString::fromStdString(save_id));
        id_item->setEditable(false);
        id_item->setData(id_item->text(), RoleSort);
        id_item->setData(gui::utils::to_qt_path(path), RoleSavePath);
        auto *note_item = new QStandardItem(note);
        note_item->setData(note, RoleSort);
        m_model->appendRow({ icon_item, title_item, modified_item, id_item, note_item });

        if (app == apps.end() || app->second.icon_path.empty())
            continue;
        const fs::path icon_path = fs::path(app->second.icon_path).is_absolute() ? fs::path(app->second.icon_path) : m_emuenv.vita_fs_path / app->second.icon_path;
        const QPersistentModelIndex index = m_model->indexFromItem(icon_item);
        auto *watcher = new QFutureWatcher<QImage>(this);
        connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, index] {
            if (index.isValid()) {
                const QPixmap pixmap = QPixmap::fromImage(watcher->result());
                m_model->setData(index, pixmap, RoleOriginalPixmap);
                m_model->setData(index, pixmap.scaled(m_icon_size, m_icon_size, Qt::KeepAspectRatio, Qt::SmoothTransformation), Qt::DecorationRole);
                if (m_table->currentIndex().isValid() && m_proxy->mapToSource(m_table->currentIndex()).row() == index.row())
                    update_details();
            }
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run([path = gui::utils::to_qt_path(icon_path)] { return QImage(path); }));
    }

    select_save(selected);
    update_details();
}

void SaveDataManagerDialog::select_save(const QString &save_id) {
    for (int row = 0; row < m_proxy->rowCount(); row++) {
        if (m_proxy->index(row, SaveId).data().toString() == save_id) {
            m_table->selectRow(row);
            m_table->scrollTo(m_proxy->index(row, SaveId));
            return;
        }
    }
}

void SaveDataManagerDialog::update_icon_size(int slider_value) {
    m_icon_size = 32 + slider_value * (160 - 32) / 100;
    m_table->verticalHeader()->setDefaultSectionSize(m_icon_size + 4);
    m_table->horizontalHeader()->resizeSection(Icon, m_icon_size + 8);
    for (int row = 0; row < m_model->rowCount(); row++) {
        QStandardItem *item = m_model->item(row, Icon);
        const QPixmap pixmap = item->data(RoleOriginalPixmap).value<QPixmap>();
        if (!pixmap.isNull())
            item->setData(pixmap.scaled(m_icon_size, m_icon_size, Qt::KeepAspectRatio, Qt::SmoothTransformation), Qt::DecorationRole);
    }
}

void SaveDataManagerDialog::update_details() {
    const QModelIndex current = m_table->currentIndex();
    const bool has_selection = current.isValid();
    m_export_button->setEnabled(has_selection);
    m_delete_button->setEnabled(has_selection);
    m_open_button->setEnabled(has_selection);
    if (!has_selection) {
        m_detail_icon->clear();
        m_detail_text->clear();
        return;
    }

    const QModelIndex icon_index = m_proxy->index(current.row(), Icon);
    const QPixmap pixmap = icon_index.data(RoleOriginalPixmap).value<QPixmap>();
    m_detail_icon->setPixmap(pixmap.isNull() ? QPixmap() : pixmap.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QString text = icon_index.data(RoleDetails).toString();
    text += QStringLiteral("<br>") + tr("Last modified: %1").arg(m_proxy->index(current.row(), Modified).data().toString());
    const QString note = m_proxy->index(current.row(), Notes).data().toString();
    if (!note.isEmpty())
        text += QStringLiteral("<br>") + tr("Note: %1").arg(note.toHtmlEscaped());
    m_detail_text->setText(text);
}

void SaveDataManagerDialog::closeEvent(QCloseEvent *event) {
    if (m_busy) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void SaveDataManagerDialog::reject() {
    if (!m_busy)
        QDialog::reject();
}

bool SaveDataManagerDialog::refuse_while_running() {
    if (!m_is_app_running || !m_is_app_running())
        return false;
    QMessageBox::warning(this, windowTitle(), tr("Stop the running application before changing save data."));
    return true;
}

bool SaveDataManagerDialog::run_with_progress(const QString &title, const std::function<bool(const std::function<void(float)> &)> &job) {
    QProgressDialog progress(title, QString(), 0, 1000, this);
    // Keeps the main window from launching an app or switching users mid conversion
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    const QPointer<QProgressDialog> guard(&progress);
    const auto report = [guard](float value) {
        QMetaObject::invokeMethod(qApp, [guard, value] {
            if (guard)
                guard->setValue(static_cast<int>(value * 1000));
        });
    };

    QFutureWatcher<bool> watcher;
    QEventLoop loop;
    connect(&watcher, &QFutureWatcher<bool>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(QtConcurrent::run([job, report] { return job(report); }));
    m_busy = true;
    loop.exec();
    m_busy = false;
    return watcher.result();
}

void SaveDataManagerDialog::import_from_vita() {
    if (refuse_while_running())
        return;

    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select a savedata folder copied from a PS Vita (ux0:user/00/savedata/<ID>)"));
    if (dir.isEmpty())
        return;

    const fs::path src = gui::utils::to_fs_path(dir);
    if (!savedata::is_encrypted(src)) {
        QMessageBox::warning(this, windowTitle(), tr("This folder has no sce_pfs/files.db, so it is not an encrypted PS Vita savedata folder.\n\nCopy the whole ux0:user/00/savedata/<ID> folder from the PS Vita without opening it."));
        return;
    }

    const QString save_id = QString::fromStdWString(src.filename().wstring());
    const fs::path dst = savedata_root() / save_id.toStdString();
    if (fs::exists(dst)
        && QMessageBox::question(this, windowTitle(), tr("Save data %1 already exists. Replace it with the imported one?").arg(save_id), QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes)
        return;

    std::string error;
    const bool ok = run_with_progress(tr("Decrypting %1...").arg(save_id), [&](const auto &progress) {
        return savedata::import_encrypted(src, dst, progress, error);
    });
    if (!ok) {
        QMessageBox::critical(this, windowTitle(), tr("Failed to decrypt the save data.\n\n%1").arg(QString::fromStdString(error)));
        return;
    }

    reload();
    select_save(save_id);
}

void SaveDataManagerDialog::export_for_vita() {
    if (refuse_while_running())
        return;

    const fs::path src = selected_save_path();
    const QString save_id = selected_save_id();
    if (src.empty())
        return;

    if (!savedata::can_export(src)) {
        QMessageBox::warning(this, windowTitle(), tr("This save data cannot be encrypted for a PS Vita.\n\n"
                                                     "Only save data that was imported from a PS Vita has the sce_sys/sealedkey, keystone and param.sfo the console needs. "
                                                     "Import the game's save data from your PS Vita once, play with it here, then export it."));
        return;
    }

    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select where to write the encrypted save data"));
    if (dir.isEmpty())
        return;

    const fs::path dst = gui::utils::to_fs_path(dir) / save_id.toStdString();
    if (is_same_or_inside(dst, src) || is_same_or_inside(dst, m_emuenv.vita_fs_path / "ux0/user")) {
        QMessageBox::warning(this, windowTitle(), tr("Choose a folder outside Vita3K's save data. Writing the encrypted copy there would replace the save data Vita3K uses."));
        return;
    }
    if (fs::exists(dst)
        && QMessageBox::question(this, windowTitle(), tr("%1 already exists. Overwrite it?").arg(QDir::toNativeSeparators(gui::utils::to_qt_path(dst))), QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes)
        return;

    std::string error;
    const bool ok = run_with_progress(tr("Encrypting %1...").arg(save_id), [&](const auto &progress) {
        return savedata::export_encrypted(src, dst, progress, error);
    });
    if (!ok) {
        QMessageBox::critical(this, windowTitle(), tr("Failed to encrypt the save data.\n\n%1").arg(QString::fromStdString(error)));
        return;
    }

    QMessageBox::information(this, windowTitle(), tr("Encrypted save data was written to:\n%1\n\nCopy this folder to ux0:user/00/savedata/ on your PS Vita, replacing the existing one.").arg(QDir::toNativeSeparators(gui::utils::to_qt_path(dst))));
}

void SaveDataManagerDialog::delete_selected() {
    if (refuse_while_running())
        return;

    const fs::path path = selected_save_path();
    if (path.empty())
        return;

    const QString title = m_proxy->index(m_table->currentIndex().row(), Icon).data(RoleAppTitle).toString();
    if (QMessageBox::question(this, windowTitle(), tr("Delete the save data of %1 (%2)?\n\nThis action cannot be undone.").arg(title, selected_save_id()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    boost::system::error_code ec;
    fs::remove_all(path, ec);
    if (ec)
        QMessageBox::critical(this, windowTitle(), tr("Failed to delete the save data.\n\n%1").arg(QString::fromStdString(ec.message())));
    reload();
}
