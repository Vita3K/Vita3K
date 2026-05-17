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

#include "ui_welcome_dialog.h"

#include <gui-qt/welcome_dialog.h>

#include <config/functions.h>
#include <config/state.h>
#include <emuenv/state.h>

#include <QActionGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
#include <QUrl>

WelcomeDialog::WelcomeDialog(EmuEnvState &emuenv, bool is_manual_show, QWidget *parent)
    : QDialog(parent)
    , emuenv(emuenv)
    , m_ui(std::make_unique<Ui::welcome_dialog>()) {
    Q_INIT_RESOURCE(resources);
    m_ui->setupUi(this);

    if (is_manual_show)
        setAttribute(Qt::WA_DeleteOnClose);

    m_ui->icon_label->setPixmap(
        QPixmap(":/Vita3K.png").scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    m_ui->label_desc->setTextFormat(Qt::PlainText);
    m_ui->label_desc->setText(tr(
        "Vita3K is an open-source PlayStation Vita emulator written in C++ for Windows, Linux, macOS and Android.\n"
        "The emulator is still in its development stages so any feedback and testing is greatly appreciated."));

    m_ui->label_info->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_ui->label_info->setText(tr(
        "<div align=\"center\">To get started, please install all PS Vita firmware files.<br><br>"
        "A comprehensive guide on how to set up Vita3K can be found on the "
        "<a href=\"https://vita3k.org/quickstart.html\">Quickstart</a> page.<br>"
        "Consult the Commercial game and Homebrew compatibility lists to see what currently runs.<br><br>"
        "Contributions are welcome! <a href=\"https://github.com/Vita3K/Vita3K\">GitHub</a><br>"
        "Additional support can be found in the #help channel on "
        "<a href=\"https://discord.gg/6aGwQzh\">Discord</a>.</div>"));

    m_ui->label_piracy->setStyleSheet(QStringLiteral("QLabel { color: #ff0000; font-weight: 600; }"));
    m_ui->label_piracy->setText(tr("Vita3K does not condone piracy. You must dump your own games."));

    {
        struct firmware_locale {
            int lang_id;
            const char *name;
            const char *code;
        };
        static constexpr firmware_locale locales[] = {
            { 0, "Japanese", "ja-jp" },
            { 1, "English (US)", "en-us" },
            { 2, "French", "fr-fr" },
            { 3, "Spanish", "es-es" },
            { 4, "German", "de-de" },
            { 5, "Italian", "it-it" },
            { 6, "Dutch", "nl-nl" },
            { 7, "Portuguese (PT)", "pt-pt" },
            { 8, "Russian", "ru-ru" },
            { 9, "Korean", "ko-kr" },
            { 10, "Chinese (Traditional)", "zh-hant-hk" },
            { 11, "Chinese (Simplified)", "zh-hans-cn" },
            { 12, "Finnish", "fi-fi" },
            { 13, "Swedish", "sv-se" },
            { 14, "Danish", "da-dk" },
            { 15, "Norwegian", "no-no" },
            { 16, "Polish", "pl-pl" },
            { 17, "Portuguese (BR)", "pt-br" },
            { 18, "English (GB)", "en-gb" },
            { 19, "Turkish", "tr-tr" },
        };

        auto *preinst_btn = new QPushButton(tr("Download Pre-Install Firmware"), this);
        auto *font_pkg_btn = new QPushButton(tr("Download Firmware Font Package"), this);
        auto *fw_btn = new QToolButton(this);
        fw_btn->setObjectName(QStringLiteral("welcome_firmware_button"));
        fw_btn->setPopupMode(QToolButton::MenuButtonPopup);
        fw_btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        fw_btn->setMinimumWidth(250);
        fw_btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        auto *lang_menu = new QMenu(fw_btn);
        auto *lang_group = new QActionGroup(lang_menu);
        lang_group->setExclusive(true);

        for (const auto &loc : locales) {
            auto *action = lang_menu->addAction(tr(loc.name));
            action->setCheckable(true);
            action->setData(QString::fromUtf8(loc.code));
            lang_group->addAction(action);
            if (loc.lang_id == emuenv.cfg.sys_lang)
                action->setChecked(true);
        }

        fw_btn->setMenu(lang_menu);

        connect(preinst_btn, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl(QStringLiteral("https://bit.ly/4hlePsX")));
        });
        connect(fw_btn, &QToolButton::clicked, this, [lang_group] {
            for (QAction *action : lang_group->actions()) {
                if (!action->isChecked())
                    continue;

                QDesktopServices::openUrl(QUrl(
                    QString("https://www.playstation.com/%1/support/hardware/psvita/system-software/")
                        .arg(action->data().toString())));
                break;
            }
        });

        connect(lang_menu, &QMenu::triggered, this, [fw_btn](QAction *action) {
            fw_btn->setText(QObject::tr("Download Firmware: %1").arg(action->text()));
        });

        connect(font_pkg_btn, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl(QStringLiteral("https://bit.ly/2P2rb0r")));
        });

        for (QAction *action : lang_group->actions()) {
            if (!action->isChecked())
                continue;

            fw_btn->setText(tr("Download Firmware: %1").arg(action->text()));
            break;
        }

        if (auto *firmware_layout = findChild<QHBoxLayout *>(QStringLiteral("firmware_layout"))) {
            firmware_layout->insertWidget(0, preinst_btn);
            firmware_layout->insertWidget(1, fw_btn);
            firmware_layout->insertWidget(2, font_pkg_btn);
        }
    }

    connect(m_ui->commercial_compatibility_list, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://vita3k.org/compatibility.html")));
    });
    connect(m_ui->homebrew_compatibility_list, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://vita3k.org/compatibility-homebrew.html")));
    });

    m_ui->show_at_startup->setChecked(emuenv.cfg.show_welcome);

    if (!is_manual_show) {
        setWindowFlag(Qt::Window, true);
        setWindowFlag(Qt::WindowCloseButtonHint, false);
    }

    connect(m_ui->show_at_startup, &QCheckBox::clicked, this, [this](bool checked) {
        this->emuenv.cfg.show_welcome = checked;
        config::serialize_config(this->emuenv.cfg, this->emuenv.cfg.config_path);
    });

    connect(m_ui->accept, &QPushButton::clicked, this, &QDialog::accept);
}

WelcomeDialog::~WelcomeDialog() = default;
