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

#include <gui-qt/apps_list_columns.h>
#include <gui-qt/apps_list_context_menu.h>
#include <gui-qt/game_compatibility.h>
#include <gui-qt/qt_utils.h>

#include <app/functions.h>
#include <compat/state.h>
#include <config/settings.h>
#include <config/state.h>
#include <config/version.h>
#include <emuenv/state.h>
#include <io/state.h>
#include <packages/license.h>
#include <packages/sce_types.h>
#include <renderer/state.h>
#include <util/log.h>

#include <include/cpu.h>
#include <include/environment.h>

#include <SDL3/SDL_cpuinfo.h>

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#include <fmt/format.h>
#include <pugixml.hpp>

static const char *ISSUES_URL = "https://github.com/Vita3K/compatibility/issues";

static QString compat_state_string(CompatibilityState state) {
    switch (state) {
    case NOTHING: return QObject::tr("Nothing");
    case BOOTABLE: return QObject::tr("Bootable");
    case INTRO: return QObject::tr("Intro");
    case MENU: return QObject::tr("Menu");
    case INGAME_LESS: return QObject::tr("In-Game (less)");
    case INGAME_MORE: return QObject::tr("In-Game (more)");
    case PLAYABLE: return QObject::tr("Playable");
    default: return QObject::tr("Unknown");
    }
}

static QString url_encode(const QString &input) {
    return QString(input.toUtf8().toPercentEncoding());
}

static quint64 directory_size(const QString &path) {
    quint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

static QString format_size(quint64 bytes) {
    constexpr double GB = 1024.0 * 1024.0 * 1024.0;
    constexpr double MB = 1024.0 * 1024.0;
    constexpr double KB = 1024.0;
    if (bytes >= GB)
        return QString("%1 GB").arg(bytes / GB, 0, 'f', 2);
    if (bytes >= MB)
        return QString("%1 MB").arg(bytes / MB, 0, 'f', 2);
    if (bytes >= KB)
        return QString("%1 KB").arg(bytes / KB, 0, 'f', 2);
    return QString("%1 B").arg(bytes);
}

AppsListContextMenu::AppsListContextMenu(EmuEnvState &emuenv,
    const std::vector<const app::AppEntry *> &apps,
    GameCompatibility *compat,
    bool app_running,
    QWidget *parent)
    : QMenu(parent)
    , m_emuenv(emuenv)
    , m_compat(compat)
    , m_app_running(app_running) {
    if (apps.size() == 1 && apps.front())
        build_single(*apps.front());
    else if (apps.size() > 1)
        build_multi(apps);
}

void AppsListContextMenu::build_single(const app::AppEntry &app) {
    m_paths = make_app_paths(app);
    add_boot_actions(app);
    addSeparator();
    add_compat_actions(app);
    add_copy_info_actions(app);
    add_custom_config_actions(app);
    addSeparator();
    add_open_folder_actions(app);
    addSeparator();
    add_delete_actions(app);
    addSeparator();
    add_misc_actions(app);
    addSeparator();
    add_information_actions(app);
}

AppsListContextMenu::AppPaths AppsListContextMenu::make_app_paths(const app::AppEntry &app) const {
    const auto &title_id = app.title_id;
    return {
        .app = m_emuenv.vita_fs_path / "ux0/app" / app.path,
        .save_data = m_emuenv.vita_fs_path / "ux0/user" / m_emuenv.io.user_id / "savedata" / app.savedata,
        .patch = m_emuenv.shared_path / "patch" / title_id,
        .addcont = m_emuenv.vita_fs_path / "ux0/addcont" / app.addcont,
        .license = m_emuenv.vita_fs_path / "ux0/license" / title_id,
        .shader_cache = m_emuenv.cache_path / "shaders" / title_id,
        .shader_log = m_emuenv.cache_path / "shaderlog" / title_id,
        .shader_log_alt = m_emuenv.log_path / "shaderlog" / title_id,
        .export_textures = m_emuenv.shared_path / "textures/export" / title_id,
        .import_textures = m_emuenv.shared_path / "textures/import" / title_id,
    };
}

void AppsListContextMenu::build_multi(const std::vector<const app::AppEntry *> &apps) {
    auto *delete_all = addAction(tr("Delete Selected Applications"));
    delete_all->setEnabled(!m_app_running);
    connect(delete_all, &QAction::triggered, this, [this, apps] {
        const int result = QMessageBox::question(parentWidget(),
            tr("Delete Applications"),
            tr("Are you sure you want to delete %1 selected applications?\n\n"
               "This action cannot be undone.")
                .arg(apps.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result != QMessageBox::Yes)
            return;
        for (const auto *app : apps) {
            if (app)
                app::delete_app(m_emuenv, app->path);
        }
        emit refresh_requested();
    });

    auto *delete_shader_cache = addAction(tr("Delete Shader Caches"));
    connect(delete_shader_cache, &QAction::triggered, this, [this, apps] {
        for (const auto *app : apps) {
            if (!app)
                continue;
            const auto path = m_emuenv.cache_path / "shaders" / app->title_id;
            if (fs::exists(path)) {
                try {
                    fs::remove_all(path);
                } catch (const std::exception &e) {
                    LOG_WARN("Failed to remove {}: {}", path, e.what());
                }
            }
        }
    });
}

void AppsListContextMenu::add_boot_actions(const app::AppEntry &app) {
    auto *boot = addAction(tr("Boot"));
    auto font = boot->font();
    font.setBold(true);
    boot->setFont(font);
    connect(boot, &QAction::triggered, this, [this, &app] {
        emit boot_requested(app);
    });

    auto *view_live_area = addAction(tr("View Live Area"));
    connect(view_live_area, &QAction::triggered, this, [this, &app] {
        emit view_live_area_requested(app);
    });
}

void AppsListContextMenu::add_compat_actions(const app::AppEntry &app) {
    const auto &title_id = app.title_id;
    const bool is_commercial = title_id.starts_with("PCS") || (title_id == "NPXS10007");
    const bool has_report = m_emuenv.compat.compat_db_loaded
        && m_emuenv.compat.app_compat_db.contains(title_id);
    const auto compat_state = has_report
        ? m_emuenv.compat.app_compat_db.at(title_id).state
        : UNKNOWN;

    auto *compat_menu = addMenu(tr("Compatibility"));

    if (!is_commercial || !m_emuenv.compat.compat_db_loaded) {
        auto *check = compat_menu->addAction(tr("Check Compatibility"));
        connect(check, &QAction::triggered, this, [title_id, &app] {
            const std::string url = title_id.starts_with("PCS")
                ? "https://vita3k.org/compatibility?g=" + title_id
                : "https://github.com/Vita3K/homebrew-compatibility/issues?q=" + app.title;
            QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
        });
    } else {
        auto *state_label = compat_menu->addAction(compat_state_string(compat_state));
        state_label->setEnabled(false);

        compat_menu->addSeparator();

        if (has_report) {
            const auto &compat_entry = m_emuenv.compat.app_compat_db.at(title_id);

            auto *open_report = compat_menu->addAction(tr("Open State Report"));
            connect(open_report, &QAction::triggered, this, [compat_entry] {
                const auto url = fmt::format("{}/{}", ISSUES_URL, compat_entry.issue_id);
                QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
            });

            auto *copy_summary = compat_menu->addAction(tr("Copy Vita3K Summary"));
            connect(copy_summary, &QAction::triggered, this, [this] {
                const auto summary = fmt::format(
                    "# Vita3K summary\n"
                    "- Version: {}\n"
                    "- Build number: {}\n"
                    "- Commit hash: https://github.com/vita3k/vita3k/commit/{}\n"
                    "- GPU backend: {}",
                    app_version, app_number, app_hash,
                    m_emuenv.cfg.backend_renderer);
                QApplication::clipboard()->setText(QString::fromStdString(summary));
            });

            auto *copy_env = compat_menu->addAction(tr("Copy Test Environment Summary"));
            connect(copy_env, &QAction::triggered, this, [this] {
#ifdef _WIN32
                const auto user = std::getenv("USERNAME");
#else
                const auto user = std::getenv("USER");
#endif
                std::string gpu_name = "Unknown";
                if (m_emuenv.renderer)
                    gpu_name = std::string(m_emuenv.renderer->get_gpu_name());

                const auto summary = fmt::format(
                    "# Test environment summary\n"
                    "- Tested by: {}\n"
                    "- OS: {}\n"
                    "- CPU: {}\n"
                    "- GPU: {}\n"
                    "- RAM: {} GB",
                    user ? user : "?",
                    CppCommon::Environment::OSVersion(),
                    CppCommon::CPU::Architecture(),
                    gpu_name,
                    SDL_GetSystemRAM() / 1000);
                QApplication::clipboard()->setText(QString::fromStdString(summary));
            });
        } else {
            auto *create_report = compat_menu->addAction(tr("Create State Report"));
            connect(create_report, &QAction::triggered, this, [this, &app, title_id] {
                const auto title = url_encode(QString::fromStdString(app.title));

                const auto app_summary = fmt::format(
                    "%23 App summary%0A"
                    "- App name: {}%0A"
                    "- App serial: {}%0A"
                    "- App version: {}",
                    app.title, title_id, app.app_ver);

                const auto vita3k_summary = fmt::format(
                    "%23 Vita3K summary%0A"
                    "- Version: {}%0A"
                    "- Build number: {}%0A"
                    "- Commit hash: https://github.com/vita3k/vita3k/commit/{}%0A"
                    "- GPU backend: {}",
                    app_version, app_number, app_hash,
                    m_emuenv.cfg.backend_renderer);

#ifdef _WIN32
                const auto user = std::getenv("USERNAME");
#else
                const auto user = std::getenv("USER");
#endif
                std::string gpu_name = "Unknown";
                if (m_emuenv.renderer)
                    gpu_name = std::string(m_emuenv.renderer->get_gpu_name());

                const auto test_env = fmt::format(
                    "%23 Test environment summary%0A"
                    "- Tested by: {} <!-- Change your username if needed -->%0A"
                    "- OS: {}%0A"
                    "- CPU: {}%0A"
                    "- GPU: {}%0A"
                    "- RAM: {} GB",
                    user ? user : "?",
                    CppCommon::Environment::OSVersion(),
                    CppCommon::CPU::Architecture(),
                    gpu_name,
                    SDL_GetSystemRAM() / 1000);

                const auto rest = "%23 Issues%0A<!-- Summary of problems -->%0A%0A"
                                  "%23 Screenshots%0A![image](https://?)%0A%0A"
                                  "%23 Log%0A%0A"
                                  "%23 Recommended labels%0A"
                                  "<!-- See https://github.com/Vita3K/compatibility/labels -->%0A"
                                  "- A?%0A- B?%0A- C?";

                const auto url = fmt::format(
                    "{}/new?assignees=&labels=&projects=&template=1-ISSUE_TEMPLATE.md&title={} [{}]&body={}%0A%0A{}%0A%0A{}%0A%0A{}",
                    ISSUES_URL, title.toStdString(), title_id,
                    app_summary, vita3k_summary, test_env, rest);

                QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
            });
        }
    }

    if (is_commercial) {
        compat_menu->addSeparator();
        auto *update_db = compat_menu->addAction(tr("Update Database"));
        connect(update_db, &QAction::triggered, this, [this] {
            if (m_compat)
                m_compat->request_update();
        });
    }
}

void AppsListContextMenu::add_copy_info_actions(const app::AppEntry &app) {
    auto *copy_menu = addMenu(tr("Copy Info"));

    auto *copy_name_id = copy_menu->addAction(tr("Name and Serial"));
    connect(copy_name_id, &QAction::triggered, this, [&app] {
        QApplication::clipboard()->setText(
            QString::fromStdString(fmt::format("{} [{}]", app.title, app.title_id)));
    });

    auto *copy_name = copy_menu->addAction(tr("Name"));
    connect(copy_name, &QAction::triggered, this, [&app] {
        QApplication::clipboard()->setText(QString::fromStdString(app.title));
    });

    auto *copy_serial = copy_menu->addAction(tr("Serial"));
    connect(copy_serial, &QAction::triggered, this, [&app] {
        QApplication::clipboard()->setText(QString::fromStdString(app.title_id));
    });

    auto *copy_summary = copy_menu->addAction(tr("App Summary"));
    connect(copy_summary, &QAction::triggered, this, [&app] {
        const auto text = fmt::format(
            "# App summary\n- App name: {}\n- App serial: {}\n- App version: {}",
            app.title, app.title_id, app.app_ver);
        QApplication::clipboard()->setText(QString::fromStdString(text));
    });
}

void AppsListContextMenu::add_custom_config_actions(const app::AppEntry &app) {
    const bool has_config = config::has_custom_config(m_emuenv.config_path, app.path);

    auto *config_menu = addMenu(tr("Custom Config"));

    if (!has_config) {
        auto *create = config_menu->addAction(tr("Create"));
        connect(create, &QAction::triggered, this, [this, &app] {
            emit open_settings_requested(app.path, 0);
        });
    } else {
        auto *edit = config_menu->addAction(tr("Edit"));
        connect(edit, &QAction::triggered, this, [this, &app] {
            emit open_settings_requested(app.path, 0);
        });

        auto *remove = config_menu->addAction(tr("Remove"));
        connect(remove, &QAction::triggered, this, [this, &app] {
            app::delete_custom_settings(m_emuenv, app.path);
            emit refresh_requested();
        });
    }
}

void AppsListContextMenu::add_open_folder_actions(const app::AppEntry &app) {
    auto *open_menu = addMenu(tr("Open Folder"));

    auto *open_app = open_menu->addAction(tr("Application"));
    connect(open_app, &QAction::triggered, this, [this] {
        gui::utils::open_dir(m_paths.app);
    });

    if (fs::exists(m_paths.save_data)) {
        auto *a = open_menu->addAction(tr("Save Data"));
        connect(a, &QAction::triggered, this, [this] {
            gui::utils::open_dir(m_paths.save_data);
        });
    }

    if (fs::exists(m_paths.patch)) {
        auto *a = open_menu->addAction(tr("Patch"));
        connect(a, &QAction::triggered, this, [this] {
            gui::utils::open_dir(m_paths.patch);
        });
    }

    if (fs::exists(m_paths.addcont)) {
        auto *a = open_menu->addAction(tr("DLC"));
        connect(a, &QAction::triggered, this, [this] {
            gui::utils::open_dir(m_paths.addcont);
        });
    }

    if (fs::exists(m_paths.license)) {
        auto *a = open_menu->addAction(tr("License"));
        connect(a, &QAction::triggered, this, [this] {
            gui::utils::open_dir(m_paths.license);
        });
    }

    if (fs::exists(m_paths.shader_cache)) {
        auto *a = open_menu->addAction(tr("Shader Cache"));
        connect(a, &QAction::triggered, this, [this] {
            gui::utils::open_dir(m_paths.shader_cache);
        });
    }

    const auto shader_log = fs::exists(m_paths.shader_log) ? m_paths.shader_log
        : fs::exists(m_paths.shader_log_alt)               ? m_paths.shader_log_alt
                                                           : fs::path{};
    if (!shader_log.empty()) {
        auto *a = open_menu->addAction(tr("Shader Log"));
        connect(a, &QAction::triggered, this, [shader_log] {
            gui::utils::open_dir(shader_log);
        });
    }

    if (fs::exists(m_paths.export_textures)) {
        auto *a = open_menu->addAction(tr("Export Textures"));
        connect(a, &QAction::triggered, this, [this] {
            gui::utils::open_dir(m_paths.export_textures);
        });
    }

    if (fs::exists(m_paths.import_textures)) {
        auto *a = open_menu->addAction(tr("Import Textures"));
        connect(a, &QAction::triggered, this, [this] {
            gui::utils::open_dir(m_paths.import_textures);
        });
    }
}

void AppsListContextMenu::add_delete_actions(const app::AppEntry &app) {
    auto *delete_menu = addMenu(tr("Delete"));

    auto *delete_app = delete_menu->addAction(tr("Application"));
    delete_app->setEnabled(!m_app_running);
    connect(delete_app, &QAction::triggered, this, [this, &app] {
        const int result = QMessageBox::question(parentWidget(),
            tr("Delete Application"),
            tr("Are you sure you want to delete %1 [%2]?\n\n"
               "This action cannot be undone.")
                .arg(QString::fromStdString(app.title),
                    QString::fromStdString(app.title_id)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result == QMessageBox::Yes) {
            app::delete_app(m_emuenv, app.path);
            emit refresh_requested();
        }
    });

    if (fs::exists(m_paths.save_data)) {
        auto *a = delete_menu->addAction(tr("Save Data"));
        connect(a, &QAction::triggered, this, [this] {
            const int r = QMessageBox::question(parentWidget(),
                tr("Delete Save Data"),
                tr("Are you sure you want to delete the save data?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (r == QMessageBox::Yes) {
                try {
                    fs::remove_all(m_paths.save_data);
                } catch (const std::exception &e) {
                    LOG_WARN("Failed to remove {}: {}", m_paths.save_data, e.what());
                }
            }
        });
    }

    if (fs::exists(m_paths.patch)) {
        auto *a = delete_menu->addAction(tr("Patch"));
        connect(a, &QAction::triggered, this, [this] {
            const int r = QMessageBox::question(parentWidget(),
                tr("Delete Patch"),
                tr("Are you sure you want to delete the patch data?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (r == QMessageBox::Yes) {
                try {
                    fs::remove_all(m_paths.patch);
                } catch (const std::exception &e) {
                    LOG_WARN("Failed to remove {}: {}", m_paths.patch, e.what());
                }
            }
        });
    }

    if (fs::exists(m_paths.addcont)) {
        auto *a = delete_menu->addAction(tr("DLC"));
        connect(a, &QAction::triggered, this, [this] {
            const int r = QMessageBox::question(parentWidget(),
                tr("Delete DLC"),
                tr("Are you sure you want to delete the DLC data?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (r == QMessageBox::Yes) {
                try {
                    fs::remove_all(m_paths.addcont);
                } catch (const std::exception &e) {
                    LOG_WARN("Failed to remove {}: {}", m_paths.addcont, e.what());
                }
            }
        });
    }

    if (fs::exists(m_paths.license)) {
        auto *a = delete_menu->addAction(tr("License"));
        connect(a, &QAction::triggered, this, [this] {
            const int r = QMessageBox::question(parentWidget(),
                tr("Delete License"),
                tr("Are you sure you want to delete the license?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (r == QMessageBox::Yes) {
                try {
                    fs::remove_all(m_paths.license);
                } catch (const std::exception &e) {
                    LOG_WARN("Failed to remove {}: {}", m_paths.license, e.what());
                }
            }
        });
    }

    if (fs::exists(m_paths.shader_cache)) {
        auto *a = delete_menu->addAction(tr("Shader Cache"));
        connect(a, &QAction::triggered, this, [this] {
            try {
                fs::remove_all(m_paths.shader_cache);
            } catch (const std::exception &e) {
                LOG_WARN("Failed to remove {}: {}", m_paths.shader_cache, e.what());
            }
        });
    }

    if (fs::exists(m_paths.shader_log)) {
        auto *a = delete_menu->addAction(tr("Shader Log"));
        connect(a, &QAction::triggered, this, [this] {
            try {
                fs::remove_all(m_paths.shader_log);
            } catch (const std::exception &e) {
                LOG_WARN("Failed to remove {}: {}", m_paths.shader_log, e.what());
            }
        });
    }

    if (fs::exists(m_paths.export_textures)) {
        auto *a = delete_menu->addAction(tr("Export Textures"));
        connect(a, &QAction::triggered, this, [this] {
            try {
                fs::remove_all(m_paths.export_textures);
            } catch (const std::exception &e) {
                LOG_WARN("Failed to remove {}: {}", m_paths.export_textures, e.what());
            }
        });
    }

    if (fs::exists(m_paths.import_textures)) {
        auto *a = delete_menu->addAction(tr("Import Textures"));
        connect(a, &QAction::triggered, this, [this] {
            try {
                fs::remove_all(m_paths.import_textures);
            } catch (const std::exception &e) {
                LOG_WARN("Failed to remove {}: {}", m_paths.import_textures, e.what());
            }
        });
    }
}

void AppsListContextMenu::add_misc_actions(const app::AppEntry &app) {
    const auto &title_id = app.title_id;

    auto *other_menu = addMenu(tr("Other"));

    auto *decrypt = other_menu->addAction(tr("Decrypt All SELF"));
    connect(decrypt, &QAction::triggered, this, [this, &app, title_id] {
        if (!m_emuenv.license.rif.contains(title_id))
            get_license(m_emuenv, title_id, app.content_id);
        decrypt_selfs(m_paths.app, m_emuenv.cache_path, m_emuenv.license.rif[title_id].key);
    });

    auto *reset_time = other_menu->addAction(tr("Reset Last Time Played"));
    connect(reset_time, &QAction::triggered, this, [this, &app] {
        app::reset_last_time_app_used(m_emuenv, app.path);
        emit refresh_requested();
    });

    const auto changeinfo_path = m_paths.app / "sce_sys/changeinfo";
    if (fs::exists(changeinfo_path) && !fs::is_empty(changeinfo_path)) {
        auto *history = other_menu->addAction(tr("Update History"));
        connect(history, &QAction::triggered, this, [this, changeinfo_path] {
            const auto lang_file = fmt::format("changeinfo_{:0>2d}.xml", m_emuenv.cfg.sys_lang);
            const auto fname = fs::exists(changeinfo_path / lang_file)
                ? lang_file
                : std::string("changeinfo.xml");

            pugi::xml_document doc;
            if (!doc.load_file((changeinfo_path / fname).c_str()))
                return;

            std::vector<std::pair<QString, QString>> entries;
            for (const auto &info : doc.child("changeinfo")) {
                const QString ver = QString::fromLatin1(info.attribute("app_ver").as_string());
                QString text = QString::fromUtf8(info.text().as_string());

                text.replace(QStringLiteral("<li>"), QStringLiteral("\u30FB"));
                text.replace(QRegularExpression(QStringLiteral("<br\\s*/?>|</li>")), QStringLiteral("\n"));
                text.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QString());
                text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
                text.replace(QStringLiteral("&reg;"), QStringLiteral("\u00AE"));

                text.replace(QRegularExpression(QStringLiteral("[ \\t]+")), QStringLiteral(" "));
                text.replace(QRegularExpression(QStringLiteral("\\n\\s*\\n")), QStringLiteral("\n"));
                text = text.trimmed();

                entries.emplace_back(ver, text);
            }

            if (entries.empty())
                return;

            auto *dlg = new QDialog(parentWidget());
            dlg->setWindowTitle(tr("Update History"));
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->setMinimumSize(500, 400);

            auto *layout = new QVBoxLayout(dlg);
            auto *scroll = new QScrollArea(dlg);
            scroll->setWidgetResizable(true);

            auto *content = new QWidget();
            auto *content_layout = new QVBoxLayout(content);

            for (auto it = entries.crbegin(); it != entries.crend(); ++it) {
                auto *ver_label = new QLabel(tr("Version %1").arg(it->first));
                auto font = ver_label->font();
                font.setBold(true);
                font.setPointSizeF(font.pointSizeF() * 1.2);
                ver_label->setFont(font);
                content_layout->addWidget(ver_label);

                auto *text_label = new QLabel(it->second);
                text_label->setWordWrap(true);
                text_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
                content_layout->addWidget(text_label);

                content_layout->addSpacing(12);
            }

            content_layout->addStretch();
            scroll->setWidget(content);
            layout->addWidget(scroll);

            auto *close_btn = new QPushButton(tr("Close"), dlg);
            connect(close_btn, &QPushButton::clicked, dlg, &QDialog::accept);
            layout->addWidget(close_btn, 0, Qt::AlignRight);

            dlg->show();
        });
    }
}

void AppsListContextMenu::add_information_actions(const app::AppEntry &app) {
    auto *info = addAction(tr("Information"));
    connect(info, &QAction::triggered, this, [this, &app] {
        const auto APP_PATH = m_emuenv.vita_fs_path / "ux0/app" / app.path;
        const auto app_dir = gui::utils::to_qt_path(APP_PATH);

        const bool has_report = m_emuenv.compat.compat_db_loaded
            && m_emuenv.compat.app_compat_db.contains(app.title_id);
        const auto compat_state = has_report
            ? m_emuenv.compat.app_compat_db.at(app.title_id).state
            : UNKNOWN;

        const quint64 size = directory_size(app_dir);

        auto *dlg = new QDialog(parentWidget());
        dlg->setWindowTitle(tr("App Information"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setMinimumWidth(420);

        auto *layout = new QVBoxLayout(dlg);

        auto *form = new QFormLayout();
        form->setLabelAlignment(Qt::AlignRight);

        auto add_row = [form](const QString &label, const QString &value) {
            auto *val = new QLabel(value);
            val->setTextInteractionFlags(Qt::TextSelectableByMouse);
            val->setWordWrap(true);
            form->addRow(new QLabel(label), val);
        };

        add_row(tr("Name"), QString::fromStdString(app.title));
        add_row(tr("Serial"), QString::fromStdString(app.title_id));
        add_row(tr("Version"), QString::fromStdString(app.app_ver));
        add_row(tr("Category"), app_category_label(app.category));
        add_row(tr("Content ID"), QString::fromStdString(app.content_id));
        add_row(tr("Parental Level"), QString::fromStdString(app.parental_level));
        add_row(tr("Compatibility"), compat_state_string(compat_state));
        add_row(tr("Size on Disk"), format_size(size));
        add_row(tr("Path"), app_dir);

        layout->addLayout(form);

        auto *close_btn = new QPushButton(tr("Close"), dlg);
        connect(close_btn, &QPushButton::clicked, dlg, &QDialog::accept);
        layout->addWidget(close_btn, 0, Qt::AlignRight);

        dlg->show();
    });
}
