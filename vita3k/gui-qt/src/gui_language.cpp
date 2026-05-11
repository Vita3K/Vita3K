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

#include <gui-qt/gui_language.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <QTranslator>

#include <array>
#include <memory>
#include <vector>

namespace gui::i18n {

namespace {

const std::array<UiLanguageOption, 23> k_ui_languages = { {
    { "en", "English (US)" },
    { "en-GB", "English (UK)" },
    { "ja", "日本語" },
    { "fr", "Français" },
    { "es", "Español" },
    { "de", "Deutsch" },
    { "it", "Italiano" },
    { "nl", "Nederlands" },
    { "pt-PT", "Português (Portugal)" },
    { "pt-BR", "Português (Brasil)" },
    { "ru", "Русский" },
    { "ko", "한국어" },
    { "zh-Hant", "繁體中文" },
    { "zh-Hans", "简体中文" },
    { "fi", "Suomi" },
    { "sv", "Svenska" },
    { "da", "Dansk" },
    { "no", "Norsk" },
    { "pl", "Polski" },
    { "tr", "Türkçe" },
    { "uk", "Українська" },
    { "id", "Bahasa Indonesia" },
    { "ms", "Bahasa Melayu" },
} };

std::unique_ptr<QTranslator> s_translator;

QStringList translation_search_paths() {
    const QString app_dir = QCoreApplication::applicationDirPath();
    return {
        app_dir + QStringLiteral("/translations"),
        app_dir + QStringLiteral("/../Resources/translations"),
    };
}

QSet<QString> available_translation_tags() {
    const QString prefix = QStringLiteral("vita3k_");

    QSet<QString> tags;
    for (const QString &path : translation_search_paths()) {
        const QDir dir(path);
        const auto entries = dir.entryList({ QStringLiteral("vita3k_*.qm"), QStringLiteral("vita3k_*.ts") }, QDir::Files);
        for (const QString &entry : entries) {
            QString tag = QFileInfo(entry).completeBaseName();
            if (!tag.startsWith(prefix))
                continue;

            tag.remove(0, prefix.size());
            tag.replace(QLatin1Char('_'), QLatin1Char('-'));
            tags.insert(tag);
        }
    }

    return tags;
}

} // namespace

std::span<const UiLanguageOption> ui_language_options() {
    static std::vector<UiLanguageOption> available_languages;

    available_languages.clear();

    const QSet<QString> tags = available_translation_tags();
    if (tags.isEmpty()) {
        available_languages.push_back(k_ui_languages.front());
        return available_languages;
    }

    for (const auto &language : k_ui_languages) {
        const QString tag = QString::fromUtf8(language.tag.data(), static_cast<int>(language.tag.size()));
        if (tags.contains(tag))
            available_languages.push_back(language);
    }

    if (available_languages.empty())
        available_languages.push_back(k_ui_languages.front());

    return available_languages;
}

QString language_name(std::string_view tag) {
    for (const auto &language : k_ui_languages) {
        if (language.tag == tag)
            return QString::fromUtf8(language.native_name.data(), static_cast<int>(language.native_name.size()));
    }

    return QString::fromUtf8(tag.data(), static_cast<int>(tag.size()));
}

bool apply_ui_language(QApplication &app, std::string_view configured_tag) {
    if (s_translator)
        app.removeTranslator(s_translator.get());

    s_translator = std::make_unique<QTranslator>();

    const auto locale = configured_tag.empty()
        ? QLocale::system()
        : QLocale(QString::fromUtf8(configured_tag.data(), static_cast<int>(configured_tag.size())));

    for (const QString &path : translation_search_paths()) {
        if (s_translator->load(locale, QStringLiteral("vita3k"), QStringLiteral("_"), path)) {
            app.installTranslator(s_translator.get());
            return true;
        }
    }

    s_translator.reset();
    return false;
}

} // namespace gui::i18n
