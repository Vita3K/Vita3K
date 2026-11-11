// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <vita_shell/functions.h>

#include <util/fs.h>
#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>
#include <util/system.h>

#include <fmt/format.h>
#include <pugixml.hpp>

#include <algorithm>
#include <system_error>

namespace vita_shell {

using Sources = std::map<std::string, std::string>;

static std::optional<std::string> stamp(const fs::path &path) {
    std::error_code error;
    const auto time = fs::last_write_time(path, error);
    if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory)
        return "missing";
    if (error) {
        LOG_ERROR("Cannot inspect theme source '{}': {}", path, error.message());
        return std::nullopt;
    }
    return fmt::format("{}", time.time_since_epoch().count());
}

static std::optional<Sources> theme_sources(const fs::path &root) {
    Sources sources;
    const auto append_stamp = [](std::string &value, const fs::path &path) {
        const auto result = stamp(path);
        if (!result)
            return false;
        if (!value.empty())
            value += ':';
        value += *result;
        return true;
    };
    const auto firmware = root / "vs0/data/internal/theme";
    std::error_code error;
    if (fs::is_directory(firmware, error) && !fs::is_empty(firmware, error)) {
        auto &value = sources["@firmware"];
        if (!append_stamp(value, firmware) || !append_stamp(value, firmware / "theme_defaultImage.png")
            || !append_stamp(value, firmware / "defaultTheme_homeScreen.png") || !append_stamp(value, firmware / "defaultTheme_startScreen.png"))
            return std::nullopt;
    } else {
        if (error && error != std::errc::no_such_file_or_directory) {
            LOG_ERROR("Cannot inspect firmware theme '{}': {}", firmware, error.message());
            return std::nullopt;
        }
        sources["@firmware"] = "missing";
    }
    const auto themes = root / "ux0/theme";
    error.clear();
    fs::directory_iterator it(themes, error), end;
    if (error && error != std::errc::no_such_file_or_directory) {
        LOG_ERROR("Cannot enumerate themes '{}': {}", themes, error.message());
        return std::nullopt;
    }
    while (!error && it != end) {
        if (it->is_directory(error)) {
            const auto path = it->path();
            const auto id = fs_utils::path_to_utf8(path.filename());
            std::string value;
            if (!append_stamp(value, path) || !append_stamp(value, path / "theme.xml")
                || !append_stamp(value, path / "sce_pfs/files.db") || !append_stamp(value, path / "sce_pfs/unicv.db"))
                return std::nullopt;
            if (id.size() == 36 && id[6] == '-' && id[16] == '_' && id[19] == '-') {
                const auto licenses = root / "ux0/license" / fs_utils::utf8_to_path(id.substr(7, 9));
                if (!append_stamp(value, licenses))
                    return std::nullopt;
                std::error_code license_error;
                fs::directory_iterator license(licenses, license_error), license_end;
                if (license_error && license_error != std::errc::no_such_file_or_directory) {
                    LOG_ERROR("Cannot enumerate theme licenses '{}': {}", licenses, license_error.message());
                    return std::nullopt;
                }
                std::map<std::string, std::string> files;
                while (!license_error && license != license_end) {
                    if (license->path().extension() == ".rif") {
                        const auto file_stamp = stamp(license->path());
                        if (!file_stamp)
                            return std::nullopt;
                        files.emplace(fs_utils::path_to_utf8(license->path().filename()), *file_stamp);
                    }
                    license.increment(license_error);
                }
                if (license_error && license_error != std::errc::no_such_file_or_directory) {
                    LOG_ERROR("Cannot enumerate theme licenses '{}': {}", licenses, license_error.message());
                    return std::nullopt;
                }
                for (const auto &[name, value_stamp] : files)
                    value += ":" + name + ":" + value_stamp;
            }
            sources.emplace(id, std::move(value));
        }
        if (!error)
            it.increment(error);
    }
    if (error && error != std::errc::no_such_file_or_directory) {
        LOG_ERROR("Cannot enumerate themes '{}': {}", themes, error.message());
        return std::nullopt;
    }
    return sources;
}

static bool load_cache(const fs::path &path, int language, const Sources &sources, ThemeCatalog &catalog) {
    if (!fs::exists(path))
        return false;
    pugi::xml_document doc;
    const auto result = doc.load_file(path.c_str());
    const auto root = doc.child("themes-cache");
    if (!result || !root || root.attribute("version").as_uint() != 1 || root.attribute("language").as_int(-1) != language) {
        LOG_WARN("Theme cache '{}' is invalid or outdated; rebuilding.", path);
        return false;
    }
    Sources cached;
    for (const auto &node : root.child("sources").children("source")) {
        if (!node.attribute("id") || !node.attribute("stamp") || !cached.emplace(node.attribute("id").as_string(), node.attribute("stamp").as_string()).second)
            return false;
    }
    if (!cached.contains("@firmware"))
        return false;
    ThemeCatalog loaded;
    for (const auto &node : root.child("themes").children("theme")) {
        const std::string id = node.attribute("id").as_string();
        if (id.empty() || (id != "default" && !cached.contains(id)) || loaded.info.contains(id)
            || !node.attribute("title") || !node.attribute("provider") || !node.attribute("version")
            || !node.attribute("size") || !node.attribute("updated") || !node.attribute("package")
            || !node.attribute("home") || !node.attribute("lock")) {
            LOG_WARN("Invalid theme entry in cache '{}'.", path);
            return false;
        }
        ThemeInfo theme;
        theme.title = node.attribute("title").as_string();
        theme.provided = node.attribute("provider").as_string();
        theme.version = node.attribute("version").as_string();
        theme.size = static_cast<size_t>(node.attribute("size").as_ullong());
        const auto updated = static_cast<std::time_t>(node.attribute("updated").as_llong());
        SAFE_LOCALTIME(&updated, &theme.updated);
        theme.preview[ThemeImage::Package] = node.attribute("package").as_string();
        theme.preview[ThemeImage::Home] = node.attribute("home").as_string();
        theme.preview[ThemeImage::Lock] = node.attribute("lock").as_string();
        loaded.info.emplace(id, std::move(theme));
        loaded.list.emplace_back(id, updated);
    }
    const auto expected = cached.size() - 1 + (cached.at("@firmware") != "missing" ? 1 : 0);
    if (loaded.info.size() != expected || (cached.at("@firmware") != "missing") != loaded.info.contains("default"))
        return false;
    for (const auto &[id, updated] : loaded.list) {
        const auto key = id == "default" ? "@firmware" : id;
        const auto current = sources.find(key);
        if (current != sources.end() && current->second == cached.at(key)) {
            catalog.info.emplace(id, std::move(loaded.info.at(id)));
            catalog.list.emplace_back(id, updated);
        }
    }
    return cached == sources;
}

static void save_cache(const fs::path &path, int language, const Sources &sources, const ThemeCatalog &catalog) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    if (error) {
        LOG_ERROR("Cannot create theme cache directory '{}': {}", path.parent_path(), error.message());
        return;
    }
    pugi::xml_document doc;
    auto root = doc.append_child("themes-cache");
    root.append_attribute("version") = 1;
    root.append_attribute("language") = language;
    auto source_nodes = root.append_child("sources");
    for (const auto &[id, value] : sources) {
        auto node = source_nodes.append_child("source");
        node.append_attribute("id") = id.c_str();
        node.append_attribute("stamp") = value.c_str();
    }
    auto theme_nodes = root.append_child("themes");
    for (const auto &[id, updated] : catalog.list) {
        const auto &theme = catalog.info.at(id);
        auto node = theme_nodes.append_child("theme");
        node.append_attribute("id") = id.c_str();
        node.append_attribute("title") = theme.title.c_str();
        node.append_attribute("provider") = theme.provided.c_str();
        node.append_attribute("version") = theme.version.c_str();
        node.append_attribute("size") = static_cast<unsigned long long>(theme.size);
        node.append_attribute("updated") = static_cast<long long>(updated);
        for (const auto &[type, name] : theme.preview)
            node.append_attribute(type == ThemeImage::Package ? "package" : type == ThemeImage::Home ? "home"
                                                                                                     : "lock") = name.c_str();
    }
    const auto temporary = path.parent_path() / "themes.xml.tmp";
    const auto backup = path.parent_path() / "themes.xml.bak";
    if (!doc.save_file(temporary.c_str())) {
        LOG_ERROR("Cannot write theme cache '{}'.", temporary);
        return;
    }
    const bool had_cache = fs::exists(path);
    if (had_cache) {
        fs::remove(backup, error);
        if (!error)
            fs::rename(path, backup, error);
    }
    if (!error)
        fs::rename(temporary, path, error);
    if (error) {
        LOG_ERROR("Cannot replace theme cache '{}': {}", path, error.message());
        if (had_cache && fs::exists(backup) && !fs::exists(path)) {
            std::error_code restore_error;
            fs::rename(backup, path, restore_error);
            if (restore_error)
                LOG_ERROR("Cannot restore theme cache '{}': {}", path, restore_error.message());
        }
        return;
    }
    if (had_cache) {
        fs::remove(backup, error);
        if (error)
            LOG_WARN("Cannot remove theme cache backup '{}': {}", backup, error.message());
    }
}

static const char *locale(int language) {
    switch (language) {
    case SCE_SYSTEM_PARAM_LANG_JAPANESE: return "m_ja";
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_US: return "m_en";
    case SCE_SYSTEM_PARAM_LANG_FRENCH: return "m_fr";
    case SCE_SYSTEM_PARAM_LANG_SPANISH: return "m_es";
    case SCE_SYSTEM_PARAM_LANG_GERMAN: return "m_de";
    case SCE_SYSTEM_PARAM_LANG_ITALIAN: return "m_it";
    case SCE_SYSTEM_PARAM_LANG_DUTCH: return "m_nl";
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT: return "m_pt";
    case SCE_SYSTEM_PARAM_LANG_RUSSIAN: return "m_ru";
    case SCE_SYSTEM_PARAM_LANG_KOREAN: return "m_ko";
    case SCE_SYSTEM_PARAM_LANG_CHINESE_T: return "m_ch";
    case SCE_SYSTEM_PARAM_LANG_CHINESE_S: return "m_zh";
    case SCE_SYSTEM_PARAM_LANG_FINNISH: return "m_fi";
    case SCE_SYSTEM_PARAM_LANG_SWEDISH: return "m_sv";
    case SCE_SYSTEM_PARAM_LANG_DANISH: return "m_da";
    case SCE_SYSTEM_PARAM_LANG_NORWEGIAN: return "m_no";
    case SCE_SYSTEM_PARAM_LANG_POLISH: return "m_pl";
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: return "m_pt-br";
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB: return "m_en-gb";
    case SCE_SYSTEM_PARAM_LANG_TURKISH: return "m_tr";
    default: return "";
    }
}
std::optional<ThemeCatalog> build_theme_catalog(const fs::path &root, int language, const std::string &default_title, IOState &io) {
    ThemeCatalog catalog;
    const auto sources = theme_sources(root);
    if (!sources)
        return std::nullopt;
    const auto cache = root / "ux0/temp/themes.xml";
    if (load_cache(cache, language, *sources, catalog)) {
        if (catalog.info.contains("default"))
            catalog.info.at("default").title = default_title;
        return catalog;
    }
    bool complete = true;
    for (const auto &[id, source] : *sources) {
        if (id == "@firmware" || catalog.info.contains(id))
            continue;
        const auto directory = root / "ux0/theme" / fs_utils::utf8_to_path(id);
        ThemeReader reader(directory, true, &io);
        std::vector<uint8_t> xml;
        pugi::xml_document doc;
        if (!reader.read("theme.xml", xml) || !doc.load_buffer(xml.data(), xml.size()) || !doc.child("theme").child("InfomationProperty")) {
            LOG_ERROR("Cannot read theme metadata for '{}'.", id);
            complete = false;
            continue;
        }
        const auto info = doc.child("theme").child("InfomationProperty");
        ThemeInfo theme;
        theme.preview[ThemeImage::Package] = info.child("m_packageImageFilePath").text().as_string();
        theme.preview[ThemeImage::Home] = info.child("m_homePreviewFilePath").text().as_string();
        theme.preview[ThemeImage::Lock] = info.child("m_startPreviewFilePath").text().as_string();
        const auto localized = [language](const pugi::xml_node &node) {
            const auto value = node.child("m_param").child(locale(language)).text();
            return std::string(value.empty() ? node.child("m_default").text().as_string() : value.as_string());
        };
        theme.title = localized(info.child("m_title"));
        theme.provided = localized(info.child("m_provider"));
        theme.version = info.child("m_contentVer").text().as_string();
        std::uintmax_t size = 0;
        for (const auto &entry : fs::recursive_directory_iterator(directory))
            if (entry.is_regular_file())
                size += entry.file_size();
        theme.size = static_cast<size_t>(size / 1024);
        const auto updated = fs_utils::to_time_t(fs::last_write_time(directory));
        SAFE_LOCALTIME(&updated, &theme.updated);
        catalog.info.emplace(id, std::move(theme));
        catalog.list.emplace_back(id, updated);
    }
    if (sources->at("@firmware") != "missing" && !catalog.info.contains("default")) {
        ThemeInfo theme;
        theme.title = default_title;
        theme.preview[ThemeImage::Package] = "data/internal/theme/theme_defaultImage.png";
        theme.preview[ThemeImage::Home] = "data/internal/theme/defaultTheme_homeScreen.png";
        theme.preview[ThemeImage::Lock] = "data/internal/theme/defaultTheme_startScreen.png";
        catalog.info.emplace("default", std::move(theme));
        catalog.list.emplace_back("default", std::time_t{});
    }
    if (catalog.info.contains("default"))
        catalog.info.at("default").title = default_title;
    std::sort(catalog.list.begin(), catalog.list.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    const auto current_sources = complete ? theme_sources(root) : std::nullopt;
    if (complete && !current_sources)
        return std::nullopt;
    if (complete && sources == current_sources)
        save_cache(cache, language, *sources, catalog);
    else if (!complete)
        LOG_WARN("Theme cache was not replaced because some themes could not be read.");
    return catalog;
}
} // namespace vita_shell