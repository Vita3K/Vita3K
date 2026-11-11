// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <vita_shell/functions.h>

#include <io/functions.h>
#include <regmgr/functions.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>
#include <vfs/mounts.h>

#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace vita_shell {

static ThemeState current_theme;

bool set_shell_theme_registry(RegMgrState &regmgr, const std::string &content_id) {
    const auto registry_path = content_id == "default" ? std::string("vs0:data/internal/theme") : "ux0:theme/" + content_id;
    uint32_t registry_size = 0;
    bool wallpaper_available = false;
    {
        std::lock_guard registry_lock(regmgr.mutex);
        const auto category = regmgr.reg_template.find("/CONFIG/THEME/");
        const auto values = regmgr.system_dreg.find("/CONFIG/THEME/");
        if (category != regmgr.reg_template.end() && values != regmgr.system_dreg.end() && values->second.contains("current_theme")) {
            const auto entry = std::find_if(category->second.begin(), category->second.end(), [](const auto &item) {
                return item.name == "current_theme" && item.type == REG_TYPE_STR;
            });
            if (entry != category->second.end())
                registry_size = entry->size;
            const auto wallpaper = values->second.find("wallpaper");
            wallpaper_available = wallpaper != values->second.end() && wallpaper->second.size() >= sizeof(int32_t)
                && std::any_of(category->second.begin(), category->second.end(), [](const auto &item) {
                       return item.name == "wallpaper" && item.type == REG_TYPE_INT;
                   });
        }
    }
    if (registry_size == 0 || registry_path.size() >= registry_size) {
        LOG_ERROR("Invalid or unavailable current_theme registry entry");
        return false;
    }
    if (!wallpaper_available) {
        LOG_ERROR("Invalid or unavailable wallpaper registry entry");
        return false;
    }
    std::vector<char> registry_value(registry_size, 0);
    std::copy(registry_path.begin(), registry_path.end(), registry_value.begin());
    regmgr::set_str_value(regmgr, "/CONFIG/THEME/", "current_theme", registry_value.data(), registry_size);
    regmgr::set_int_value(regmgr, "/CONFIG/THEME/", "wallpaper", 2);
    return true;
}

bool apply_shell_theme(IOState &io, RegMgrState &regmgr, const fs::path &vita_fs_path, const std::string &content_id) {
    static std::mutex mutex;
    static std::atomic_uint64_t sequence{ 0 };
    const std::lock_guard lock(mutex);
    const auto shell = vita_fs_path / "ur0" / "shell";
    const auto target = shell / "theme";
    fs::path staging;
    fs::path backup;
    bool published = false;
    bool backed_up = false;
    const auto apply = [&]() -> bool {
        fs::create_directory(vita_fs_path / "ur0");
        if (!fs_utils::is_plain_component(vita_fs_path / "ur0")) {
            LOG_ERROR("Invalid ur0 directory for theme '{}'", content_id);
            return false;
        }
        fs::create_directory(shell);
        if (!fs_utils::is_plain_component(vita_fs_path / "ur0") || !fs_utils::is_plain_component(shell)) {
            LOG_ERROR("Invalid ur0 shell directory for theme '{}'", content_id);
            return false;
        }
        const auto status = fs::symlink_status(target);
        if (fs::exists(status) && (!fs::is_directory(status) || !fs_utils::is_plain_component(target))) {
            LOG_ERROR("Invalid active theme directory '{}'", target);
            return false;
        }
        for (int attempt = 0; attempt < 128; ++attempt) {
            const auto candidate = shell / (".vita3k-theme-" + std::to_string(sequence++));
            if (fs::create_directory(candidate)) {
                staging = candidate;
                break;
            }
        }
        if (staging.empty()) {
            LOG_ERROR("Cannot create staging directory for theme '{}'", content_id);
            return false;
        }
        if (content_id != "default") {
            if (!pfsmgr::copy_theme_data(io, vita_fs_path, content_id, staging)) {
                LOG_ERROR("Cannot copy mounted theme '{}'", content_id);
                return false;
            }
            pugi::xml_document document;
            if (!document.load_file((staging / "theme.xml").c_str()) || !document.child("theme")) {
                LOG_ERROR("Invalid decrypted theme.xml for '{}'", content_id);
                return false;
            }
            const auto xml_id = document.child("theme").child("InfomationProperty").child("m_contentId").text().as_string();
            if (*xml_id && content_id != xml_id) {
                LOG_ERROR("Theme content ID '{}' does not match selected directory '{}'", xml_id, content_id);
                return false;
            }
        }
        if (fs::exists(status)) {
            backup = staging;
            backup += "-backup";
            if (!fs::create_directory(backup)) {
                LOG_ERROR("Cannot reserve theme backup directory '{}'", backup);
                return false;
            }
            fs::rename(target, backup / "previous");
            backed_up = true;
        }
        if (content_id != "default") {
            fs::rename(staging, target);
            staging.clear();
            published = true;
        }
        if (!set_shell_theme_registry(regmgr, content_id)) {
            LOG_ERROR("Cannot update current_theme in the registry for '{}'", content_id);
            return false;
        }
        if (!staging.empty()) {
            std::error_code cleanup_error;
            fs::remove(staging, cleanup_error);
            if (cleanup_error)
                LOG_WARN("Cannot remove theme staging directory '{}': {}", staging, cleanup_error.message());
            staging.clear();
        }
        if (!backup.empty()) {
            std::error_code cleanup_error;
            fs::remove_all(backup, cleanup_error);
            if (cleanup_error)
                LOG_WARN("Cannot remove theme backup '{}': {}", backup, cleanup_error.message());
        }
        return true;
    };
    try {
        if (apply())
            return true;
    } catch (const std::runtime_error &error) {
        LOG_ERROR("Cannot apply theme '{}' for the Vita Shell: {}", content_id, error.what());
    }
    std::error_code cleanup_error;
    if (published) {
        fs::remove_all(target, cleanup_error);
        if (cleanup_error)
            LOG_ERROR("Cannot remove failed active theme '{}': {}", target, cleanup_error.message());
    }
    if (backed_up) {
        std::error_code restore_error;
        fs::rename(backup / "previous", target, restore_error);
        if (restore_error)
            LOG_ERROR("Cannot restore active theme; original retained at '{}': {}", backup / "previous", restore_error.message());
    }
    if (!staging.empty()) {
        fs::remove_all(staging, cleanup_error);
        if (cleanup_error)
            LOG_WARN("Cannot remove theme staging directory '{}': {}", staging, cleanup_error.message());
    }
    if (!backup.empty()) {
        cleanup_error.clear();
        fs::remove(backup, cleanup_error);
        if (cleanup_error)
            LOG_WARN("Theme backup retained at '{}': {}", backup, cleanup_error.message());
    }
    return false;
}

ThemeReader::ThemeReader(const fs::path &directory, bool read_pfs, IOState *io)
    : directory_(directory) {
    if (!read_pfs)
        return;
    if (!io || !pfsmgr::mount_theme_data(*io, directory, mount_point_, protected_)) {
        LOG_ERROR("Cannot mount theme '{}'", directory);
        valid_ = false;
        return;
    }
    io_ = io;
    owns_mount_ = true;
}

ThemeReader::~ThemeReader() {
    if (owns_mount_ && !pfsmgr::unmount_theme_data(*io_, mount_point_))
        LOG_WARN("Cannot unmount temporary theme '{}'", mount_point_);
}

bool ThemeReader::read(const fs::path &relative_path, std::vector<uint8_t> &data) const {
    data.clear();
    const auto path = relative_path.lexically_normal();
    if (!valid_ || path.empty() || path.has_root_path() || path == "." || *path.begin() == "..") {
        LOG_WARN("Invalid or unavailable theme resource '{}'.", relative_path);
        return false;
    }
    if (!io_)
        return fs_utils::read_data(directory_ / path, data);
    std::string name = mount_point_;
    for (const auto &part : path)
        name += "/" + fs_utils::path_to_utf8(part);
    const size_t max_size = protected_ ? 64 * 1024 * 1024 : std::numeric_limits<size_t>::max();
    return read_file_to_buffer(*io_, name.c_str(), directory_.parent_path().parent_path().parent_path(), data, "theme", max_size);
}

constexpr std::array<std::string_view, 7> system_app_ids = {
    "NPXS10003",
    "NPXS10006",
    "NPXS10008",
    "NPXS10014",
    "NPXS10015",
    "NPXS10026",
    "NPXS19999"
};

static std::optional<Color> parse_color(std::string value, std::string_view context) {
    value = string_utils::trim_copy(value);
    if (value.size() < 6) {
        if (!value.empty()) {
            LOG_WARN("Invalid theme color '{}' for {}; expected at least six hexadecimal digits.", value, context);
            return Color{};
        }
        return std::nullopt;
    }

    const auto hex = value.substr(value.size() - 6);
    if (hex.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
        LOG_WARN("Invalid theme color '{}' for {}; expected six hexadecimal digits.", value, context);
        return Color{};
    }

    const auto rgb = static_cast<uint32_t>(std::strtoul(hex.c_str(), nullptr, 16));
    return Color{
        static_cast<uint8_t>((rgb >> 16) & 0xff),
        static_cast<uint8_t>((rgb >> 8) & 0xff),
        static_cast<uint8_t>(rgb & 0xff),
        255
    };
}

static std::optional<Color> parse_background_text_color(const std::string &value) {
    if (value.empty())
        return std::nullopt;
    const auto rgb = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 16));
    return Color{
        static_cast<uint8_t>((rgb >> 16) & 0xff),
        static_cast<uint8_t>((rgb >> 8) & 0xff),
        static_cast<uint8_t>(rgb & 0xff),
        255
    };
}

static std::string child_text(const pugi::xml_node &parent, const char *name) {
    return string_utils::trim_copy(parent.child(name).text().as_string());
}

static std::string normalize_locale(std::string locale) {
    locale = string_utils::trim_copy(locale);
    std::ranges::replace(locale, '_', '-');
    std::ranges::transform(locale, locale.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return locale;
}

static std::string localized_text(const pugi::xml_node &node, const std::string &preferred_locale) {
    if (!node)
        return {};

    const auto locale = normalize_locale(preferred_locale);
    const auto try_key = [&node](const std::string &name) {
        return child_text(node, name.c_str());
    };
    const auto try_locale = [&try_key](const std::string &name) {
        if (name.empty())
            return std::string{};
        if (const auto exact = try_key(name); !exact.empty())
            return exact;
        return try_key("m_" + name);
    };

    if (!locale.empty()) {
        if (const auto exact = try_locale(locale); !exact.empty())
            return exact;
        if (const auto separator = locale.find('-'); separator != std::string::npos) {
            if (const auto base = try_locale(locale.substr(0, separator)); !base.empty())
                return base;
        }
    }

    if (const auto fallback = child_text(node, "m_default"); !fallback.empty())
        return fallback;
    return {};
}

static std::optional<std::filesystem::path> resolve_asset_path(const std::filesystem::path &theme_directory, std::string raw_path) {
    raw_path = string_utils::trim_copy(raw_path);
    std::ranges::replace(raw_path, '\\', '/');
    if (raw_path.empty())
        return std::nullopt;
    if (raw_path.size() >= 2 && std::isalpha(static_cast<unsigned char>(raw_path[0])) && raw_path[1] == ':')
        return std::nullopt;
    if (raw_path.find(':') != std::string::npos)
        return std::nullopt;
    while (raw_path.starts_with("./"))
        raw_path.erase(0, 2);
    while (raw_path.starts_with('/'))
        raw_path.erase(0, 1);

    const auto relative_path = fs_utils::utf8_to_path(raw_path).lexically_normal();
    if (relative_path.empty() || relative_path.is_absolute() || relative_path.has_root_path()
        || relative_path == "." || *relative_path.begin() == "..")
        return std::nullopt;
    return (theme_directory / relative_path).lexically_normal();
}

static void collect_directory_metrics(const std::filesystem::path &root, ThemeState &state) {
    std::error_code error;
    if (const auto root_time = fs::last_write_time(root, error); !error)
        state.updated_time = fs_utils::to_time_t(root_time);

    fs::recursive_directory_iterator end;
    for (fs::recursive_directory_iterator it(root, error); !error && it != end; it.increment(error)) {
        const auto path = it->path();
        if (fs::is_regular_file(path, error)) {
            state.installed_size += fs::file_size(path, error);
            error.clear();
        }
        if (const auto write_time = fs::last_write_time(path, error); !error)
            state.updated_time = std::max(state.updated_time, fs_utils::to_time_t(write_time));
        error.clear();
    }
}

static std::filesystem::path get_default_icon_path(const std::filesystem::path &vita_fs_path,
    const std::filesystem::path &static_assets_path) {
    const auto firmware_icon = vita_fs_path / "vs0/data/internal/livearea/default/sce_sys/icon0.png";
    if (fs::is_regular_file(firmware_icon))
        return firmware_icon;
    return static_assets_path / "data/image/icon.png";
}

static void set_bgm(ThemeState &state, const std::filesystem::path &vita_fs_path,
    std::string device, std::filesystem::path device_path) {
    state.bgm.device = std::move(device);
    state.bgm.device_path = std::move(device_path);
    state.bgm.path = vita_fs_path / state.bgm.device / state.bgm.device_path;
}

ThemeState load_theme_directory(const std::filesystem::path &theme_directory,
    const std::string &preferred_locale) {
    ThemeState state;
    state.installed_path = theme_directory;
    state.content_id = fs_utils::path_to_utf8(theme_directory.filename());
    state.name = state.content_id;

    const auto theme_xml_path = theme_directory / "theme.xml";
    std::vector<uint8_t> xml_buffer;
    if (!fs_utils::read_data(theme_xml_path, xml_buffer)) {
        LOG_WARN("Failed to read Vita theme metadata from '{}'.", theme_xml_path);
        return state;
    }

    pugi::xml_document document;
    const auto parse_result = document.load_buffer(xml_buffer.data(), xml_buffer.size());
    if (!parse_result) {
        LOG_WARN("Failed to parse Vita theme metadata from '{}': {}.", theme_xml_path, parse_result.description());
        return state;
    }

    const auto theme = document.child("theme");
    if (!theme) {
        LOG_WARN("Theme metadata at '{}' is missing the root <theme> node.", theme_xml_path);
        return state;
    }

    state.loaded = true;
    const auto info = theme.child("InfomationProperty");
    const auto info_bar = theme.child("InfomationBarProperty");
    const auto home = theme.child("HomeProperty");
    const auto start_screen = theme.child("StartScreenProperty");
    state.content_id = child_text(info, "m_contentId");
    state.name = localized_text(info.child("m_title"), preferred_locale);
    state.provider = localized_text(info.child("m_provider"), preferred_locale);
    state.content_version = child_text(info, "m_contentVer");
    if (state.name.empty())
        state.name = state.content_id.empty() ? fs_utils::path_to_utf8(theme_directory.filename()) : state.content_id;

    const auto package_image = child_text(info, "m_packageImageFilePath");
    const auto preview_image = package_image.empty() ? child_text(info, "m_previewFilePath") : package_image;
    if (const auto path = resolve_asset_path(theme_directory, preview_image); path && fs::exists(*path))
        state.package_thumbnail_path = *path;

    const std::array<std::pair<std::string_view, std::string_view>, 7> system_icon_paths = { { { "NPXS10003", "m_browser" },
        { "NPXS10006", "m_friend" },
        { "NPXS10008", "m_trophy" },
        { "NPXS10014", "m_message" },
        { "NPXS10015", "m_settings" },
        { "NPXS10026", "m_hostCollabo" },
        { "NPXS19999", "m_power" } } };
    for (const auto &[title_id, node_name] : system_icon_paths) {
        const auto icon_name = child_text(home.child(node_name.data()), "m_iconFilePath");
        if (icon_name.empty())
            continue;
        if (const auto path = resolve_asset_path(theme_directory, icon_name)) {
            state.system_app_icons[std::string(title_id)] = *path;
            if (!fs::is_regular_file(*path))
                LOG_WARN("Theme icon '{}' was not found for '{}'.", icon_name, state.content_id);
        } else {
            LOG_WARN("Theme icon '{}' was not found for '{}'.", icon_name, state.content_id);
        }
    }

    const auto load_page_indicator = [&](const std::string_view node_name, std::filesystem::path &target) {
        const auto icon_name = child_text(home, node_name.data());
        if (icon_name.empty())
            return;
        if (const auto path = resolve_asset_path(theme_directory, icon_name); path && fs::is_regular_file(*path))
            target = *path;
        else
            LOG_WARN("Theme page indicator '{}' was not found for '{}'.", icon_name, state.content_id);
    };
    load_page_indicator("m_basePageFilePath", state.page_indicators.base);
    load_page_indicator("m_curPageFilePath", state.page_indicators.current);

    const auto bgm_name = child_text(home, "m_bgmFilePath");
    if (!bgm_name.empty()) {
        if (const auto path = resolve_asset_path(theme_directory, bgm_name)) {
            state.bgm.device = "ux0";
            state.bgm.device_path = std::filesystem::path("theme") / theme_directory.filename() / fs_utils::utf8_to_path(bgm_name);
            if (fs::is_regular_file(*path))
                state.bgm.path = *path;
            else
                LOG_WARN("Skipping missing Vita theme BGM '{}' for '{}'.", bgm_name, state.content_id);
        } else {
            LOG_WARN("Skipping missing Vita theme BGM '{}' for '{}'.", bgm_name, state.content_id);
        }
    }

    for (const auto &background_node : home.child("m_bgParam")) {
        const auto image_name = child_text(background_node, "m_imageFilePath");
        if (image_name.empty())
            continue;
        const auto path = resolve_asset_path(theme_directory, image_name);
        if (!path || !fs::is_regular_file(*path)) {
            LOG_WARN("Skipping missing Vita home background '{}' for '{}'.", image_name, state.content_id);
            continue;
        }
        state.backgrounds.push_back({ image_name, *path, parse_background_text_color(child_text(background_node, "m_fontColor")) });
    }

    const auto bar_color = child_text(info_bar, "m_barColor");
    state.information_bar.bar_color = parse_color(bar_color, "information bar");
    const auto indicator_color = child_text(info_bar, "m_indicatorColor");
    state.information_bar.indicator_color = parse_color(indicator_color, "information bar indicator");
    const auto notice_font_color = child_text(info_bar, "m_noticeFontColor");
    state.information_bar.notice_font_color = parse_color(notice_font_color, "information bar notice font");

    const std::array<std::pair<NoticeIcon, std::string_view>, 2> notice_paths = { { { NoticeIcon::NoNotice, "m_noNoticeFilePath" },
        { NoticeIcon::NewNotice, "m_newNoticeFilePath" } } };
    for (const auto &[icon_type, node_name] : notice_paths) {
        const auto icon_name = child_text(info_bar, node_name.data());
        if (icon_name.empty())
            continue;
        const auto path = resolve_asset_path(theme_directory, icon_name);
        if (path && fs::is_regular_file(*path))
            state.information_bar.notice_icons.emplace(icon_type, *path);
        else
            LOG_WARN("Theme information bar icon '{}' was not found for '{}'.", icon_name, state.content_id);
    }

    state.start_notice_background_color = parse_color(child_text(start_screen, "m_notifyBgColor"), "start screen notification background");
    state.start_notice_border_color = parse_color(child_text(start_screen, "m_notifyBorderColor"), "start screen notification border");
    state.start_notice_font_color = parse_color(child_text(start_screen, "m_notifyFontColor"), "start screen notification font");
    state.start_date_color = parse_color(child_text(start_screen, "m_dateColor"), "start screen date");
    if (const auto date_layout = start_screen.child("m_dateLayout"); date_layout)
        state.start_date_layout = static_cast<StartDateLayout>(date_layout.text().as_int());

    const auto start_name = child_text(start_screen, "m_filePath");
    if (!start_name.empty()) {
        if (const auto path = resolve_asset_path(theme_directory, start_name)) {
            state.start_screen_background = *path;
            if (!fs::is_regular_file(*path))
                LOG_WARN("Vita lockscreen background '{}' was not found for '{}'.", start_name, state.content_id);
        } else {
            LOG_WARN("Invalid Vita lockscreen background path '{}' for '{}'.", start_name, state.content_id);
        }
    }

    collect_directory_metrics(theme_directory, state);
    return state;
}

std::vector<ThemeState> enumerate_themes(const std::filesystem::path &themes_root,
    const std::string &preferred_locale) {
    std::vector<ThemeState> themes;
    if (!fs::exists(themes_root) || !fs::is_directory(themes_root))
        return themes;

    std::vector<std::filesystem::path> theme_paths;
    for (fs::directory_iterator it(themes_root), end; it != end; ++it) {
        if (fs::is_directory(it->path()))
            theme_paths.push_back(it->path());
    }
    std::ranges::sort(theme_paths, [](const auto &lhs, const auto &rhs) {
        return fs_utils::path_to_utf8(lhs.filename()) < fs_utils::path_to_utf8(rhs.filename());
    });

    for (const auto &theme_path : theme_paths) {
        auto theme = load_theme_directory(theme_path, preferred_locale);
        if (theme.loaded && (!theme.backgrounds.empty() || !theme.start_screen_background.empty()))
            themes.push_back(std::move(theme));
    }
    return themes;
}

static ThemeState build_theme_state(const std::filesystem::path &vita_fs_path,
    const std::filesystem::path &static_assets_path, const std::string &content_id) {
    ThemeState state;
    state.loaded = true;
    state.content_id = content_id;
    state.name = content_id == "default" ? "Default" : content_id;
    set_bgm(state, vita_fs_path, "pd0", "data/systembgm/home.at9");

    const auto default_icon = get_default_icon_path(vita_fs_path, static_assets_path);
    for (const auto title_id : system_app_ids) {
        const auto app_icon = vita_fs_path / "vs0/app" / title_id / "sce_sys/icon0.png";
        state.system_app_icons.emplace(std::string(title_id),
            fs::is_regular_file(app_icon) ? app_icon : default_icon);
    }

    if (content_id == "default") {
        constexpr std::array<std::string_view, 5> default_background_ids = {
            "NPXS10002",
            "NPXS10006",
            "NPXS10013",
            "NPXS10018",
            "NPXS10098"
        };
        for (const auto app_id : default_background_ids) {
            const auto path = vita_fs_path / "vs0/app" / app_id / "sce_sys/pic0.png";
            if (fs::is_regular_file(path))
                state.backgrounds.push_back({ std::string(app_id), path, std::nullopt });
        }

        const auto default_start = vita_fs_path / "vs0/data/internal/keylock/keylock.png";
        if (fs::is_regular_file(default_start))
            state.start_screen_background = default_start;
        else
            LOG_WARN("Default start background not found at '{}'.", default_start);
        return state;
    }

    const auto theme_directory = vita_fs_path / "ur0/shell/theme";
    auto parsed_theme = load_theme_directory(theme_directory);
    state.loaded = parsed_theme.loaded;
    if (state.loaded && !parsed_theme.content_id.empty() && parsed_theme.content_id != content_id) {
        LOG_WARN("Active theme copy contains '{}' but '{}' was requested", parsed_theme.content_id, content_id);
        state.loaded = false;
    }
    state.name = parsed_theme.name.empty() ? state.name : parsed_theme.name;
    state.provider = parsed_theme.provider;
    state.content_version = parsed_theme.content_version;
    state.installed_path = parsed_theme.installed_path;
    state.package_thumbnail_path = parsed_theme.package_thumbnail_path;
    state.backgrounds = parsed_theme.backgrounds;
    state.page_indicators = parsed_theme.page_indicators;
    if (!parsed_theme.bgm.path.empty()) {
        state.bgm = parsed_theme.bgm;
        state.bgm.device = "ur0";
        state.bgm.device_path = fs::path("shell/theme") / parsed_theme.bgm.path.lexically_relative(theme_directory);
    }
    state.information_bar = parsed_theme.information_bar;
    state.start_screen_background = parsed_theme.start_screen_background;
    state.start_date_color = parsed_theme.start_date_color;
    state.start_notice_background_color = parsed_theme.start_notice_background_color;
    state.start_notice_border_color = parsed_theme.start_notice_border_color;
    state.start_notice_font_color = parsed_theme.start_notice_font_color;
    state.start_date_layout = parsed_theme.start_date_layout;

    for (const auto &[title_id, icon_path] : parsed_theme.system_app_icons) {
        if (fs::is_regular_file(icon_path))
            state.system_app_icons[title_id] = icon_path;
        else
            state.system_app_icons[title_id] = default_icon;
    }

    if (state.start_screen_background.empty()) {
        const auto default_start = vita_fs_path / "vs0/data/internal/keylock/keylock.png";
        if (fs::is_regular_file(default_start))
            state.start_screen_background = default_start;
        else
            LOG_WARN("Default start background not found at '{}'.", default_start);
    }

    return state;
}

void load_theme(const std::filesystem::path &vita_fs_path,
    const std::filesystem::path &static_assets_path, const std::string &content_id) {
    current_theme = build_theme_state(vita_fs_path, static_assets_path, content_id);
}

ThemeState prepare_theme(const std::filesystem::path &vita_fs_path,
    const std::filesystem::path &static_assets_path, const std::string &content_id) {
    return build_theme_state(vita_fs_path, static_assets_path, content_id);
}

void clear_theme() {
    current_theme = {};
}

const ThemeState &get_theme_state() {
    return current_theme;
}

} // namespace vita_shell
