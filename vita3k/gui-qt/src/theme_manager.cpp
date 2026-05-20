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

#include <gui-qt/theme_manager.h>

#include <gui-qt/gui_settings.h>
#include <gui-qt/qt_utils.h>
#include <gui-qt/vita_theme.h>

#include <util/log.h>

#include <QApplication>
#include <QAudioOutput>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QSet>
#include <QStyle>
#include <QStyleFactory>
#include <QUrl>

#include <QRegularExpression>
#include <algorithm>

namespace {

constexpr int default_vita_theme_readability = 55;
constexpr bool default_vita_theme_normalize_fonts = true;
constexpr int default_vita_theme_cycle_interval_seconds = 15;
constexpr int default_theme_bgm_volume = 50;

const QPalette &fusion_palette() {
    static const QPalette palette = []() {
        if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
            const QPalette result = fusion->standardPalette();
            delete fusion;
            return result;
        }

        return QApplication::palette();
    }();

    return palette;
}

QString fallback_theme_name() {
    return gui::DarkStylesheet;
}

QString generated_vita_theme_name_for_id(const QString &theme_id) {
    return QStringLiteral("vita-theme-%1").arg(theme_id);
}

const gui::VitaThemeBackgroundOption *find_background_option(
    const gui::VitaThemeInfo &theme,
    const QString &background_id);

QStringList sanitize_cycle_background_ids(const QStringList &ids, const QString &anchor_background_id) {
    QStringList sanitized;
    QSet<QString> seen;

    const auto append_unique = [&sanitized, &seen](const QString &id) {
        if (!id.isEmpty() && !seen.contains(id)) {
            sanitized.append(id);
            seen.insert(id);
        }
    };

    append_unique(anchor_background_id);
    for (const QString &id : ids)
        append_unique(id);

    return sanitized;
}

gui::VitaThemeSelection sanitize_vita_theme_selection(gui::VitaThemeSelection selection) {
    selection.readability = std::clamp(selection.readability, 0, 100);
    selection.cycle_interval_seconds = std::clamp(
        selection.cycle_interval_seconds,
        5,
        120);
    selection.cycle_background_ids = sanitize_cycle_background_ids(
        selection.cycle_background_ids,
        selection.background_id);
    return selection;
}

QString resolve_theme_background_id(
    const gui::VitaThemeInfo &theme,
    const QString &background_id) {
    if (theme.background_options.empty())
        return {};

    if (const auto *background = find_background_option(theme, background_id))
        return QString::fromStdString(background->id);

    return QString::fromStdString(theme.background_options.front().id);
}

QStringList canonicalize_cycle_background_ids(
    const gui::VitaThemeInfo &theme,
    const QStringList &background_ids,
    const QString &primary_background_id) {
    QStringList canonical_ids;

    if (theme.background_options.empty())
        return canonical_ids;

    if (background_ids.isEmpty()) {
        for (const auto &background : theme.background_options)
            canonical_ids.append(QString::fromStdString(background.id));
    } else {
        QSet<QString> requested_ids;
        for (const QString &id : background_ids) {
            if (!id.isEmpty())
                requested_ids.insert(id);
        }

        for (const auto &background : theme.background_options) {
            const QString id = QString::fromStdString(background.id);
            if (requested_ids.contains(id))
                canonical_ids.append(id);
        }
    }

    if (canonical_ids.isEmpty()) {
        if (!primary_background_id.isEmpty()) {
            canonical_ids.append(primary_background_id);
        } else {
            canonical_ids.append(QString::fromStdString(theme.background_options.front().id));
        }
    }

    if (!primary_background_id.isEmpty()) {
        canonical_ids.removeAll(primary_background_id);
        canonical_ids.prepend(primary_background_id);
    }

    return canonical_ids;
}

gui::VitaThemeSelection normalize_selection(
    const gui::VitaThemeInfo &theme,
    gui::VitaThemeSelection selection) {
    selection = sanitize_vita_theme_selection(std::move(selection));
    selection.theme_id = QString::fromStdString(theme.theme_id);

    if (theme.background_options.empty()) {
        selection.background_id.clear();
        selection.cycle_background_ids.clear();
        selection.cycle_enabled = false;
        return selection;
    }

    selection.background_id = resolve_theme_background_id(theme, selection.background_id);
    selection.cycle_background_ids = canonicalize_cycle_background_ids(
        theme,
        selection.cycle_background_ids,
        selection.background_id);
    return selection;
}

gui::ThemeBackgroundPresentation build_background_presentation(
    const gui::VitaThemeInfo &theme,
    const gui::VitaThemeSelection &selection) {
    const gui::VitaThemeSelection canonical_selection = normalize_selection(theme, selection);

    gui::ThemeBackgroundPresentation presentation;
    presentation.interval_ms = std::clamp(canonical_selection.cycle_interval_seconds, 5, 120) * 1000;
    presentation.animated = canonical_selection.cycle_enabled;

    QSet<QString> added_paths;
    for (const QString &background_id : canonical_selection.cycle_background_ids) {
        const auto *background = find_background_option(theme, background_id);
        if (!background)
            continue;

        const QString path = gui::utils::to_qt_path(background->asset_path);
        if (!path.isEmpty() && !added_paths.contains(path)) {
            presentation.image_paths.append(path);
            added_paths.insert(path);
        }
    }

    if (presentation.image_paths.isEmpty()) {
        if (const auto *background = find_background_option(theme, canonical_selection.background_id))
            presentation.image_paths.append(gui::utils::to_qt_path(background->asset_path));
    }

    if (!presentation.animated && presentation.image_paths.size() > 1)
        presentation.image_paths = QStringList{ presentation.image_paths.front() };

    if (presentation.image_paths.size() < 2)
        presentation.animated = false;

    return presentation;
}

QJsonObject serialize_vita_theme_selection(const gui::VitaThemeSelection &selection) {
    QJsonObject object;
    object.insert(QStringLiteral("themeId"), selection.theme_id);
    object.insert(QStringLiteral("backgroundId"), selection.background_id);
    object.insert(QStringLiteral("readability"), selection.readability);
    object.insert(QStringLiteral("normalizeFontColors"), selection.normalize_font_colors);
    object.insert(QStringLiteral("cycleEnabled"), selection.cycle_enabled);
    object.insert(QStringLiteral("cycleIntervalSeconds"), selection.cycle_interval_seconds);

    QJsonArray cycle_background_ids;
    for (const QString &id : selection.cycle_background_ids)
        cycle_background_ids.append(id);
    object.insert(QStringLiteral("cycleBackgroundIds"), cycle_background_ids);
    return object;
}

std::optional<gui::VitaThemeSelection> deserialize_vita_theme_selection(const QString &serialized) {
    if (serialized.isEmpty())
        return std::nullopt;

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(serialized.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;

    const QJsonObject object = document.object();
    gui::VitaThemeSelection selection;
    selection.theme_id = object.value(QStringLiteral("themeId")).toString();
    selection.background_id = object.value(QStringLiteral("backgroundId")).toString();
    selection.readability = object.contains(QStringLiteral("readability"))
        ? object.value(QStringLiteral("readability")).toInt(default_vita_theme_readability)
        : default_vita_theme_readability;
    selection.normalize_font_colors = object.contains(QStringLiteral("normalizeFontColors"))
        ? object.value(QStringLiteral("normalizeFontColors")).toBool(default_vita_theme_normalize_fonts)
        : default_vita_theme_normalize_fonts;
    selection.cycle_enabled = object.contains(QStringLiteral("cycleEnabled"))
        ? object.value(QStringLiteral("cycleEnabled")).toBool(false)
        : false;
    selection.cycle_interval_seconds = object.contains(QStringLiteral("cycleIntervalSeconds"))
        ? object.value(QStringLiteral("cycleIntervalSeconds")).toInt(default_vita_theme_cycle_interval_seconds)
        : default_vita_theme_cycle_interval_seconds;

    const QJsonArray cycle_background_ids = object.value(QStringLiteral("cycleBackgroundIds")).toArray();
    for (const QJsonValue &id : cycle_background_ids) {
        const QString value = id.toString();
        if (!value.isEmpty())
            selection.cycle_background_ids.append(value);
    }

    if (selection.theme_id.isEmpty())
        return std::nullopt;

    return sanitize_vita_theme_selection(std::move(selection));
}

std::optional<gui::VitaThemeSelection> load_vita_theme_selection(
    const std::shared_ptr<GuiSettings> &gui_settings,
    const GuiSave &key) {
    if (!gui_settings)
        return std::nullopt;

    return deserialize_vita_theme_selection(gui_settings->get_value(key).toString());
}

void save_vita_theme_selection(
    const std::shared_ptr<GuiSettings> &gui_settings,
    const GuiSave &key,
    const gui::VitaThemeSelection &selection,
    const bool sync) {
    if (!gui_settings)
        return;

    const QString serialized = QString::fromUtf8(
        QJsonDocument(serialize_vita_theme_selection(selection)).toJson(QJsonDocument::Compact));
    gui_settings->set_value(key, serialized, false);
    if (sync)
        gui_settings->sync();
}

void clear_vita_theme_selection(
    const std::shared_ptr<GuiSettings> &gui_settings,
    const GuiSave &key,
    const bool sync) {
    if (!gui_settings)
        return;

    gui_settings->remove_value(key, false);
    if (sync)
        gui_settings->sync();
}

const gui::VitaThemeBackgroundOption *find_background_option(
    const gui::VitaThemeInfo &theme,
    const QString &background_id) {
    const auto it = std::find_if(theme.background_options.begin(), theme.background_options.end(),
        [&background_id](const gui::VitaThemeBackgroundOption &background) {
            return QString::fromStdString(background.id) == background_id;
        });
    if (it == theme.background_options.end())
        return nullptr;

    return &(*it);
}

struct GeneratedVitaThemeMetadata {
    QString template_version;
    QString theme_id;
    QString background_id;
    int readability = default_vita_theme_readability;
    bool normalize_font_colors = default_vita_theme_normalize_fonts;
};

std::optional<GeneratedVitaThemeMetadata> read_generated_vita_theme_metadata(const fs::path &qss_path) {
    QFile file(gui::utils::to_qt_path(qss_path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::nullopt;

    const QString stylesheet = QString::fromUtf8(file.readAll());
    const qsizetype start = stylesheet.indexOf(QStringLiteral("/*!VITA3K_GENERATED"));
    if (start < 0)
        return std::nullopt;

    const qsizetype end = stylesheet.indexOf(QStringLiteral("*/"), start);
    if (end < 0)
        return std::nullopt;

    GeneratedVitaThemeMetadata metadata;
    const QString block = stylesheet.mid(start, end - start);
    const QStringList lines = block.split(QLatin1Char('\n'));
    for (const QString &raw_line : lines) {
        const QString line = raw_line.trimmed();
        if (!line.startsWith(QStringLiteral("* ")))
            continue;

        const QString entry = line.mid(2);
        const qsizetype colon = entry.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;

        const QString key = entry.first(colon).trimmed();
        const QString value = entry.sliced(colon + 1).trimmed();
        if (key == QStringLiteral("template-version"))
            metadata.template_version = value;
        else if (key == QStringLiteral("theme-id"))
            metadata.theme_id = value;
        else if (key == QStringLiteral("background-id"))
            metadata.background_id = value;
        else if (key == QStringLiteral("readability"))
            metadata.readability = value.toInt();
        else if (key == QStringLiteral("normalize-font-colors"))
            metadata.normalize_font_colors = (value == QStringLiteral("true"));
    }

    return metadata;
}

} // namespace

ThemeManager::ThemeManager(std::shared_ptr<GuiSettings> gui_settings,
    QObject *parent)
    : QObject(parent)
    , m_gui_settings(std::move(gui_settings))
    , m_vita_theme_bgm_audio_output(new QAudioOutput(this))
    , m_vita_theme_bgm_player(new QMediaPlayer(this)) {
    m_vita_theme_bgm_player->setAudioOutput(m_vita_theme_bgm_audio_output);
    m_vita_theme_bgm_player->setLoops(QMediaPlayer::Infinite);
}

QList<gui::ThemeEntry> ThemeManager::available_themes() const {
    QList<gui::ThemeEntry> themes = {
        *builtin_theme(gui::LightStylesheet),
        *builtin_theme(gui::DarkStylesheet),
    };

    if (const auto generated = generated_vita_theme())
        themes.append(*generated);

    for (const auto &theme : custom_themes())
        themes.append(theme);

    return themes;
}

QString ThemeManager::current_theme_name() const {
    if (!m_current_theme_name.isEmpty())
        return m_current_theme_name;

    const QString requested_name = requested_theme_name();
    if (requested_name.isEmpty() || !resolve_theme(requested_name))
        return {};

    return requested_name;
}

bool ThemeManager::apply_theme(const QString &name, const bool force_reload) {
    const QString requested_name = name.isEmpty() ? requested_theme_name() : name;
    if (requested_name.isEmpty()) {
        LOG_ERROR("No GUI theme is configured");
        return false;
    }

    auto try_apply = [this, force_reload](const QString &theme_name, const bool log_fallback_context) -> bool {
        const auto theme = resolve_theme(theme_name);
        if (!theme) {
            if (log_fallback_context)
                LOG_ERROR("Could not resolve GUI theme '{}'", theme_name.toUtf8().constData());
            return false;
        }

        const auto stylesheet = load_stylesheet(*theme);
        if (!stylesheet) {
            if (log_fallback_context) {
                LOG_ERROR("Could not load GUI theme '{}' from {}",
                    theme->name.toUtf8().constData(),
                    theme->qss_source.toUtf8().constData());
            }
            return false;
        }

        return apply_theme_entry(*theme, *stylesheet, force_reload);
    };

    if (try_apply(requested_name, true))
        return true;

    const QString fallback_name = fallback_theme_name();
    if (requested_name == fallback_name)
        return false;

    LOG_WARN("Falling back to packaged GUI theme '{}'", fallback_name.toUtf8().constData());
    return try_apply(fallback_name, false);
}

void ThemeManager::set_vita_themes_root(const fs::path &themes_root) {
    if (m_vita_themes_root != themes_root)
        refresh_vita_theme_catalog();
    m_vita_themes_root = themes_root;
}

void ThemeManager::refresh_vita_theme_catalog() {
    m_installed_vita_theme_cache.clear();
    m_vita_theme_cache_loaded = false;
}

const std::vector<gui::VitaThemeInfo> &ThemeManager::installed_vita_themes(const bool force_reload) const {
    if (force_reload || !m_vita_theme_cache_loaded) {
        m_installed_vita_theme_cache = m_vita_themes_root.empty()
            ? std::vector<gui::VitaThemeInfo>{}
            : gui::enumerate_installed_vita_themes(m_vita_themes_root);
        m_vita_theme_cache_loaded = true;
    }

    return m_installed_vita_theme_cache;
}

void ThemeManager::ensure_generated_theme_ready(const fs::path &themes_root) {
    if (!m_gui_settings)
        return;

    set_vita_themes_root(themes_root);

    const auto selection = applied_vita_theme_selection();
    if (!selection)
        return;

    const QString expected_theme_name = generated_vita_theme_name(selection->theme_id);
    if (requested_theme_name() != expected_theme_name)
        return;

    const auto *theme = find_installed_vita_theme(selection->theme_id);
    if (!theme) {
        switch_to_fallback_theme(tr("the installed Vita theme \"%1\" could not be found").arg(selection->theme_id));
        return;
    }

    const auto *background = find_background_option(*theme, selection->background_id);
    if (!background) {
        switch_to_fallback_theme(tr("the selected Vita background \"%1\" is no longer available").arg(selection->background_id));
        return;
    }

    const fs::path generated_qss_path = generated_vita_theme_path(selection->theme_id);
    if (generated_vita_theme_needs_regeneration(*theme, *background, *selection, generated_qss_path)) {
        const auto synthesized = gui::synthesize_vita_theme_qss(
            *theme,
            background->id,
            selection->readability,
            selection->normalize_font_colors,
            gui::utils::to_fs_path(m_gui_settings->get_settings_dir()));
        if (!synthesized) {
            switch_to_fallback_theme(tr("the generated Vita theme stylesheet could not be rebuilt"));
            return;
        }
    }
}

std::optional<gui::VitaThemeSelection> ThemeManager::applied_vita_theme_selection() const {
    auto selection = load_vita_theme_selection(m_gui_settings, gui::vt_appliedSelection);
    if (!selection)
        return std::nullopt;

    if (const auto *theme = find_installed_vita_theme(selection->theme_id))
        return normalize_selection_for_theme(*theme, std::move(*selection));

    return sanitize_vita_theme_selection(std::move(*selection));
}

gui::VitaThemeSelection ThemeManager::normalize_selection_for_theme(
    const gui::VitaThemeInfo &theme,
    gui::VitaThemeSelection selection) const {
    return normalize_selection(theme, std::move(selection));
}

bool ThemeManager::apply_vita_theme_selection(
    const gui::VitaThemeSelection &selection,
    const bool force_reload) {
    if (!m_gui_settings || m_vita_themes_root.empty()) {
        LOG_ERROR("Cannot apply Vita theme selection without GUI settings and a theme root");
        return false;
    }

    const auto *theme = find_installed_vita_theme(selection.theme_id);
    if (!theme) {
        LOG_ERROR("Could not find installed Vita theme '{}'", selection.theme_id.toUtf8().constData());
        return false;
    }

    gui::VitaThemeSelection normalized_selection = normalize_selection_for_theme(*theme, selection);
    if (normalized_selection.background_id.isEmpty()) {
        LOG_ERROR("Could not resolve a background for Vita theme '{}'",
            normalized_selection.theme_id.toUtf8().constData());
        return false;
    }

    const auto *background = find_background_option(*theme, normalized_selection.background_id);
    if (!background) {
        LOG_ERROR("Could not find background '{}' for Vita theme '{}'",
            normalized_selection.background_id.toUtf8().constData(),
            normalized_selection.theme_id.toUtf8().constData());
        return false;
    }

    const auto synthesized = gui::synthesize_vita_theme_qss(
        *theme,
        background->id,
        normalized_selection.readability,
        normalized_selection.normalize_font_colors,
        gui::utils::to_fs_path(m_gui_settings->get_settings_dir()));
    if (!synthesized)
        return false;

    save_applied_selection(normalized_selection);
    m_gui_settings->set_value(
        gui::m_currentStylesheet,
        generated_vita_theme_name(normalized_selection.theme_id),
        false);
    m_gui_settings->sync();
    Q_EMIT theme_catalog_changed();

    const QFileInfo info(gui::utils::to_qt_path(synthesized->qss_path));
    const gui::ThemeEntry generated_theme{
        generated_vita_theme_name(normalized_selection.theme_id),
        generated_vita_theme_display_name(normalized_selection.theme_id),
        info.absoluteFilePath(),
        info.absolutePath(),
    };

    return apply_theme_entry(generated_theme, force_reload);
}

void ThemeManager::save_applied_selection(const gui::VitaThemeSelection &selection) const {
    gui::VitaThemeSelection persisted_selection = sanitize_vita_theme_selection(selection);
    if (const auto *theme = find_installed_vita_theme(persisted_selection.theme_id))
        persisted_selection = normalize_selection_for_theme(*theme, std::move(persisted_selection));

    save_vita_theme_selection(
        m_gui_settings,
        gui::vt_appliedSelection,
        persisted_selection,
        false);
}

void ThemeManager::clear_applied_vita_theme() {
    const bool had_selection = applied_vita_theme_selection().has_value();
    clear_vita_theme_selection(m_gui_settings, gui::vt_appliedSelection, false);
    if (m_gui_settings)
        m_gui_settings->sync();
    m_current_theme_name.clear();
    m_applied_theme_state.reset();
    refresh_vita_theme_bgm();
    if (had_selection) {
        Q_EMIT theme_catalog_changed();
        Q_EMIT theme_state_changed();
    }
}

void ThemeManager::switch_to_fallback_theme(const QString &reason) {
    const QString fallback_name = fallback_theme_name();
    LOG_WARN("Falling back to packaged GUI theme '{}' because {}",
        fallback_name.toUtf8().constData(),
        reason.toUtf8().constData());

    clear_applied_vita_theme();
    if (!m_gui_settings)
        return;

    m_gui_settings->set_value(gui::m_currentStylesheet, fallback_name, false);
    m_gui_settings->sync();
}

QString ThemeManager::requested_theme_name() const {
    if (!m_gui_settings)
        return {};

    return m_gui_settings->get_value(gui::m_currentStylesheet).toString();
}

std::optional<gui::ThemeEntry> ThemeManager::builtin_theme(const QString &name) const {
    if (name == gui::LightStylesheet) {
        return gui::ThemeEntry{
            gui::LightStylesheet,
            tr("Light", "Stylesheets"),
            QStringLiteral(":/themes/light/light.qss"),
            QStringLiteral(":/themes/light"),
        };
    }

    if (name == gui::DarkStylesheet) {
        return gui::ThemeEntry{
            gui::DarkStylesheet,
            tr("Dark", "Stylesheets"),
            QStringLiteral(":/themes/dark/dark.qss"),
            QStringLiteral(":/themes/dark"),
        };
    }

    return std::nullopt;
}

std::optional<gui::ThemeEntry> ThemeManager::generated_vita_theme() const {
    const auto selection = applied_vita_theme_selection();
    if (!selection)
        return std::nullopt;

    const fs::path path = generated_vita_theme_path(selection->theme_id);
    const QFileInfo info(gui::utils::to_qt_path(path));
    if (!info.exists() || !info.isFile())
        return std::nullopt;

    return gui::ThemeEntry{
        generated_vita_theme_name(selection->theme_id),
        generated_vita_theme_display_name(selection->theme_id),
        info.absoluteFilePath(),
        info.absolutePath(),
    };
}

const gui::VitaThemeInfo *ThemeManager::find_installed_vita_theme(
    const QString &theme_id,
    const bool force_reload) const {
    const auto &themes = installed_vita_themes(force_reload);
    const auto it = std::find_if(themes.begin(), themes.end(),
        [&theme_id](const gui::VitaThemeInfo &theme) {
            return QString::fromStdString(theme.theme_id) == theme_id;
        });
    return (it != themes.end()) ? &(*it) : nullptr;
}

bool ThemeManager::generated_vita_theme_needs_regeneration(
    const gui::VitaThemeInfo &theme,
    const gui::VitaThemeBackgroundOption &background,
    const gui::VitaThemeSelection &selection,
    const fs::path &qss_path) const {
    boost::system::error_code error_code;
    if (!fs::exists(qss_path, error_code) || error_code)
        return true;

    const auto metadata = read_generated_vita_theme_metadata(qss_path);
    if (!metadata
        || metadata->template_version != QString::fromUtf8(gui::VitaThemeTemplateVersion.data(), static_cast<int>(gui::VitaThemeTemplateVersion.size()))
        || metadata->theme_id != selection.theme_id
        || metadata->background_id != selection.background_id
        || metadata->readability != selection.readability
        || metadata->normalize_font_colors != selection.normalize_font_colors) {
        return true;
    }

    const auto generated_timestamp = fs::last_write_time(qss_path, error_code);
    if (error_code)
        return true;

    for (const fs::path &source_path : { theme.theme_xml_path, background.asset_path }) {
        if (source_path.empty())
            continue;

        if (!fs::exists(source_path, error_code) || error_code)
            return true;

        if (fs::last_write_time(source_path, error_code) > generated_timestamp || error_code)
            return true;
    }

    return false;
}

QList<gui::ThemeEntry> ThemeManager::custom_themes() const {
    QList<gui::ThemeEntry> themes;
    if (!m_gui_settings)
        return themes;

    const QStringList name_filter = { QStringLiteral("*.qss") };
    QDirIterator it(m_gui_settings->get_settings_dir(), name_filter, QDir::Files);
    while (it.hasNext()) {
        it.next();

        const QString base_name = it.fileInfo().completeBaseName();
        if (base_name == gui::LightStylesheet || base_name == gui::DarkStylesheet)
            continue;

        themes.append({
            base_name,
            base_name,
            it.fileInfo().absoluteFilePath(),
            it.fileInfo().absolutePath(),
        });
    }

    return themes;
}

std::optional<gui::ThemeEntry> ThemeManager::resolve_theme(const QString &name) const {
    if (const auto builtin = builtin_theme(name))
        return builtin;

    if (const auto generated = generated_vita_theme(); generated && generated->name == name)
        return generated;

    const auto themes = custom_themes();
    for (const auto &theme : themes) {
        if (theme.name == name)
            return theme;
    }

    return std::nullopt;
}

std::optional<gui::AppliedThemeState> ThemeManager::current_applied_theme_state() const {
    return m_applied_theme_state;
}

std::optional<gui::AppliedThemeState> ThemeManager::build_applied_theme_state(const gui::ThemeEntry &theme) const {
    gui::AppliedThemeState state{
        theme.name,
        theme.display_name,
        false,
        {},
    };

    if (m_vita_themes_root.empty())
        return state;

    const auto selection = applied_vita_theme_selection();
    if (!selection || theme.name != generated_vita_theme_name(selection->theme_id))
        return state;

    const auto *vita_theme = find_installed_vita_theme(selection->theme_id);
    if (!vita_theme)
        return state;

    state.is_vita_theme = true;
    state.background = build_background_presentation(*vita_theme, *selection);
    return state;
}

bool ThemeManager::apply_theme_entry(const gui::ThemeEntry &theme, const bool force_reload) {
    const auto stylesheet = load_stylesheet(theme);
    if (!stylesheet)
        return false;

    return apply_theme_entry(theme, *stylesheet, force_reload);
}

bool ThemeManager::apply_theme_entry(const gui::ThemeEntry &theme, const QString &stylesheet, const bool force_reload) {
    if (!force_reload && m_current_theme_name == theme.name)
        return true;

    const auto palette = parse_palette(stylesheet);
    const QString resolved_stylesheet = resolve_stylesheet_urls(stylesheet, theme);

    qApp->setStyleSheet(QString());
    ensure_fusion_style();
    QApplication::setPalette(palette.value_or(fusion_palette()));

    LOG_INFO("Loaded stylesheet '{}' from {}", theme.name.toUtf8().constData(), theme.qss_source.toUtf8().constData());
    qApp->setStyleSheet(resolved_stylesheet);
    gui::utils::invalidate_theme_cache();
    m_current_theme_name = theme.name;
    m_applied_theme_state = build_applied_theme_state(theme);
    refresh_vita_theme_bgm();
    Q_EMIT theme_state_changed();
    return true;
}

void ThemeManager::refresh_vita_theme_bgm() {
    if (!m_vita_theme_bgm_audio_output || !m_vita_theme_bgm_player || m_vita_themes_root.empty()) {
        stop_vita_theme_bgm();
        return;
    }

    const bool bgm_enabled = m_gui_settings
        ? m_gui_settings->get_value(gui::vt_bgmEnabled).toBool()
        : false;
    const auto selection = applied_vita_theme_selection();
    if (!bgm_enabled || m_vita_theme_bgm_blocked || !selection
        || !m_applied_theme_state || !m_applied_theme_state->is_vita_theme) {
        stop_vita_theme_bgm();
        return;
    }

    const auto *theme = find_installed_vita_theme(selection->theme_id);
    if (!theme || !theme->bgm_path || theme->bgm_path->empty()) {
        stop_vita_theme_bgm();
        return;
    }

    const QString source_path = gui::utils::to_qt_path(*theme->bgm_path);
    const int volume = std::clamp(
        m_gui_settings && m_gui_settings->contains_value(gui::vt_bgmVolume)
            ? m_gui_settings->get_value(gui::vt_bgmVolume).toInt()
            : default_theme_bgm_volume,
        0, 100);
    m_vita_theme_bgm_audio_output->setVolume(static_cast<float>(volume) / 100.0f);

    if (m_current_vita_theme_bgm_source_path != source_path) {
        m_current_vita_theme_bgm_source_path = source_path;
        m_vita_theme_bgm_player->stop();
        m_vita_theme_bgm_player->setSource(QUrl::fromLocalFile(source_path));
    }

    if (m_vita_theme_bgm_player->playbackState() != QMediaPlayer::PlayingState) {
        LOG_INFO("Starting Vita theme BGM playback from {}", source_path.toUtf8().constData());
        m_vita_theme_bgm_player->play();
    }
}

void ThemeManager::set_vita_theme_bgm_blocked(const bool blocked) {
    if (m_vita_theme_bgm_blocked == blocked)
        return;

    m_vita_theme_bgm_blocked = blocked;
    refresh_vita_theme_bgm();
}

void ThemeManager::stop_vita_theme_bgm() {
    if (!m_vita_theme_bgm_player)
        return;

    if (m_vita_theme_bgm_player->playbackState() != QMediaPlayer::StoppedState)
        m_vita_theme_bgm_player->stop();

    if (!m_current_vita_theme_bgm_source_path.isEmpty()) {
        m_vita_theme_bgm_player->setSource(QUrl());
        m_current_vita_theme_bgm_source_path.clear();
    }
}

std::optional<QString> ThemeManager::load_stylesheet(const gui::ThemeEntry &theme) const {
    QFile file(theme.qss_source);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::nullopt;

    return QString::fromUtf8(file.readAll());
}

std::optional<QPalette> ThemeManager::parse_palette(const QString &stylesheet) const {
    const qsizetype start = stylesheet.indexOf(QStringLiteral("/*!PALETTE"));
    if (start < 0)
        return std::nullopt;

    const qsizetype end = stylesheet.indexOf(QStringLiteral("*/"), start);
    if (end < 0)
        return std::nullopt;

    QPalette palette = fusion_palette();
    const QString block = stylesheet.mid(start, end - start);
    const QStringList lines = block.split(QLatin1Char('\n'));
    for (const QString &raw_line : lines) {
        const QString line = raw_line.trimmed();
        if (!line.startsWith(QStringLiteral("* ")))
            continue;

        const QString entry = line.mid(2);
        const qsizetype colon = entry.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;

        const QString key = entry.first(colon).trimmed();
        const QString value = entry.sliced(colon + 1).trimmed();
        if (key == QStringLiteral("dark"))
            continue;

        const QColor color(value);
        if (!color.isValid()) {
            LOG_WARN("Invalid palette color '{}' for key '{}'", value.toUtf8().constData(), key.toUtf8().constData());
            continue;
        }

        if (key == QStringLiteral("window"))
            palette.setColor(QPalette::Window, color);
        else if (key == QStringLiteral("window-text"))
            palette.setColor(QPalette::WindowText, color);
        else if (key == QStringLiteral("base"))
            palette.setColor(QPalette::Base, color);
        else if (key == QStringLiteral("alternate-base"))
            palette.setColor(QPalette::AlternateBase, color);
        else if (key == QStringLiteral("tooltip-base"))
            palette.setColor(QPalette::ToolTipBase, color);
        else if (key == QStringLiteral("tooltip-text"))
            palette.setColor(QPalette::ToolTipText, color);
        else if (key == QStringLiteral("text"))
            palette.setColor(QPalette::Text, color);
        else if (key == QStringLiteral("button"))
            palette.setColor(QPalette::Button, color);
        else if (key == QStringLiteral("button-text"))
            palette.setColor(QPalette::ButtonText, color);
        else if (key == QStringLiteral("bright-text"))
            palette.setColor(QPalette::BrightText, color);
        else if (key == QStringLiteral("link"))
            palette.setColor(QPalette::Link, color);
        else if (key == QStringLiteral("highlight"))
            palette.setColor(QPalette::Highlight, color);
        else if (key == QStringLiteral("inactive-highlight"))
            palette.setColor(QPalette::Inactive, QPalette::Highlight, color);
        else if (key == QStringLiteral("disabled-highlight"))
            palette.setColor(QPalette::Disabled, QPalette::Highlight, color);
        else if (key == QStringLiteral("highlight-text"))
            palette.setColor(QPalette::HighlightedText, color);
        else if (key == QStringLiteral("inactive-highlight-text"))
            palette.setColor(QPalette::Inactive, QPalette::HighlightedText, color);
        else if (key == QStringLiteral("disabled-highlight-text"))
            palette.setColor(QPalette::Disabled, QPalette::HighlightedText, color);
        else if (key == QStringLiteral("placeholder-text"))
            palette.setColor(QPalette::PlaceholderText, color);
        else if (key == QStringLiteral("disabled-button-text"))
            palette.setColor(QPalette::Disabled, QPalette::ButtonText, color);
        else if (key == QStringLiteral("disabled-window-text"))
            palette.setColor(QPalette::Disabled, QPalette::WindowText, color);
        else if (key == QStringLiteral("disabled-text"))
            palette.setColor(QPalette::Disabled, QPalette::Text, color);
        else
            LOG_WARN("Unknown palette key '{}'", key.toUtf8().constData());
    }

    return palette;
}

QString ThemeManager::resolve_stylesheet_urls(const QString &stylesheet, const gui::ThemeEntry &theme) const {
    static const QRegularExpression url_pattern(
        QStringLiteral(R"(url\(\s*(['"]?)([^'")]+)\1\s*\))"));

    QString resolved;
    resolved.reserve(stylesheet.size());

    qsizetype last_end = 0;
    auto it = url_pattern.globalMatch(stylesheet);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const qsizetype start = match.capturedStart(0);
        const qsizetype finish = match.capturedEnd(0);
        resolved.append(stylesheet.mid(last_end, start - last_end));
        resolved.append(QStringLiteral("url(\"%1\")").arg(resolve_stylesheet_url(match.captured(2), theme)));
        last_end = finish;
    }

    resolved.append(stylesheet.mid(last_end));
    return resolved;
}

QString ThemeManager::resolve_stylesheet_url(const QString &url, const gui::ThemeEntry &theme) const {
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty() || is_absolute_stylesheet_url(trimmed))
        return trimmed;

    if (theme.base_path.startsWith(QStringLiteral(":/"))
        || theme.base_path.startsWith(QStringLiteral("qrc:/"))) {
        const QString base_path = theme.base_path.endsWith(QLatin1Char('/'))
            ? theme.base_path.chopped(1)
            : theme.base_path;
        return base_path + QStringLiteral("/") + trimmed;
    }

    const QDir base_dir(theme.base_path);
    return QDir::fromNativeSeparators(QFileInfo(base_dir.filePath(trimmed)).absoluteFilePath());
}

bool ThemeManager::is_absolute_stylesheet_url(const QString &url) const {
    if (url.startsWith(QStringLiteral(":/"))
        || url.startsWith(QStringLiteral("qrc:/"))
        || url.startsWith(QStringLiteral("file:/"))
        || url.startsWith(QStringLiteral("/"))
        || url.startsWith(QStringLiteral("\\"))) {
        return true;
    }

    static const QRegularExpression windows_drive_prefix(QStringLiteral(R"(^[A-Za-z]:[/\\])"));
    return windows_drive_prefix.match(url).hasMatch();
}

fs::path ThemeManager::generated_vita_theme_path(const QString &theme_id) const {
    if (!m_gui_settings || theme_id.isEmpty())
        return {};

    return gui::utils::to_fs_path(m_gui_settings->get_settings_dir())
        / "vita-themes" / gui::utils::to_fs_path(theme_id) / "generated.qss";
}

QString ThemeManager::generated_vita_theme_name(const QString &theme_id) const {
    return generated_vita_theme_name_for_id(theme_id);
}

QString ThemeManager::generated_vita_theme_display_name(const QString &theme_id) const {
    if (!theme_id.isEmpty() && !m_vita_themes_root.empty()) {
        const auto *theme = find_installed_vita_theme(theme_id);
        if (theme)
            return tr("Vita Theme: %1").arg(QString::fromStdString(theme->title));
    }

    return tr("Vita Theme: %1").arg(theme_id.isEmpty() ? tr("Generated") : theme_id);
}

void ThemeManager::ensure_fusion_style() const {
    if (const QStyle *style = QApplication::style(); style && style->name().compare(QStringLiteral("Fusion"), Qt::CaseInsensitive) == 0)
        return;

    if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        QApplication::setStyle(fusion);
}
