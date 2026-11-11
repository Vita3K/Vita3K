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

#include <app/functions.h>
#include <app/state.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <io/VitaIoDevice.h>
#include <io/device.h>
#include <io/state.h>
#include <packages/functions.h>
#include <packages/sfo.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>
#include <v3kn/activity.h>
#include <vfs/appmeta.h>

#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <vector>

static constexpr uint32_t CACHE_VERSION = 2;

namespace app {

namespace {

struct AppCacheSource {
    std::string app_path;
    int64_t param_sfo_write_time{ -1 };
    int64_t icon_write_time{ -1 };
    int64_t patch_sfo_write_time{ -1 };
    int64_t patch_icon_write_time{ -1 };
};

static std::string get_icon_path(const EmuEnvState &emuenv, const std::string &app_path, const std::string &title_id);

static std::string canonical_app_path(const std::string &app_path) {
    if (app_path.empty())
        return {};
    if (device::get_device(app_path) != VitaIoDevice::_INVALID)
        return app_path;
    if (app_path.find(':') != std::string::npos) {
        LOG_ERROR("Invalid application device in '{}'.", app_path);
        return {};
    }
    return "ux0:app/" + app_path;
}

static std::string canonical_time_app_path(const std::string &app_path) {
    constexpr std::array<std::string_view, 6> emulated_ids = {
        "NPXS10003", "NPXS10006", "NPXS10008", "NPXS10014", "NPXS10015", "NPXS10026"
    };
    if (app_path == "NPXS19999" || app_path == "emu:vsh/shell")
        return "vs0:vsh/shell";
    if (std::ranges::find(emulated_ids, app_path) != emulated_ids.end())
        return "emu:app/" + app_path;
    if (app_path.starts_with("emu:"))
        return app_path;
    return canonical_app_path(app_path);
}

static fs::path get_apps_cache_path(const EmuEnvState &emuenv) {
    return emuenv.config_path / "gui-configs" / "apps-cache.xml";
}

static int64_t get_path_write_time(const fs::path &path) {
    if (!fs::exists(path))
        return -1;

    try {
        return static_cast<int64_t>(fs::last_write_time(path).time_since_epoch().count());
    } catch (const std::exception &e) {
        LOG_WARN("Failed to read timestamp for '{}': {}", path, e.what());
        return -1;
    }
}

static AppCacheSource make_app_cache_source(const EmuEnvState &emuenv, const std::string &vita_path,
    const fs::path &host_path, VitaIoDevice app_device, const fs::path &relative_path) {
    AppCacheSource source{
        .app_path = vita_path,
        .param_sfo_write_time = get_path_write_time(host_path / "sce_sys/param.sfo"),
        .icon_write_time = get_path_write_time(host_path / "sce_sys/icon0.png"),
    };
    if (app_device == VitaIoDevice::vs0)
        return source;

    const auto folder_name = relative_path.filename().string();
    vfs::FileBuffer param;
    sfo::SfoAppInfo info;
    const auto actual_title_id = vfs::read_file(app_device, param, emuenv.vita_fs_path, relative_path / "sce_sys/param.sfo")
            && sfo::get_param_info(info, param, emuenv.cfg.sys_lang) && !info.app_title_id.empty()
        ? info.app_title_id
        : folder_name;
    const auto patch_path = emuenv.vita_fs_path / "ux0" / "patch" / actual_title_id / "sce_sys";
    source.patch_sfo_write_time = get_path_write_time(patch_path / "param.sfo");
    source.patch_icon_write_time = get_path_write_time(patch_path / "icon0.png");
    return source;
}

static std::optional<AppCacheSource> firmware_app_source(const EmuEnvState &emuenv, const std::string &vita_path) {
    const auto app_device = device::get_device(vita_path);
    if (app_device != VitaIoDevice::pd0 && app_device != VitaIoDevice::vs0) {
        LOG_WARN("Ignoring firmware application with invalid device: '{}'.", vita_path);
        return std::nullopt;
    }
    const fs::path relative_path = device::remove_device_from_path(vita_path, app_device);
    if (relative_path.parent_path() != fs::path("app") || relative_path.filename().empty() || relative_path.filename() == "." || relative_path.filename() == "..") {
        LOG_WARN("Ignoring invalid firmware application path '{}'.", vita_path);
        return std::nullopt;
    }
    const auto host_path = device::construct_emulated_path(app_device, relative_path, emuenv.vita_fs_path);
    if (!fs::is_regular_file(host_path / "eboot.bin")) {
        LOG_WARN("Firmware app not found at '{}'.", vita_path);
        return std::nullopt;
    }
    return make_app_cache_source(emuenv, vita_path, host_path, app_device, relative_path);
}

static std::vector<AppCacheSource> collect_app_cache_sources(const EmuEnvState &emuenv) {
    const fs::path app_path{ emuenv.vita_fs_path / "ux0/app" };
    std::vector<AppCacheSource> sources;
    if (fs::exists(app_path)) {
        for (const auto &entry : fs::directory_iterator(app_path)) {
            if (entry.path().empty() || !fs::is_directory(entry.path()))
                continue;
            if (entry.path().filename().string().ends_with("_dec"))
                continue;

            const auto folder = entry.path().filename().generic_string();
            sources.push_back(make_app_cache_source(emuenv, "ux0:app/" + folder, entry.path(), VitaIoDevice::ux0, fs::path("app") / folder));
        }
    }

    for (const auto &vita_path : emuenv.cfg.firmware_apps) {
        if (std::ranges::any_of(sources, [&](const AppCacheSource &source) { return source.app_path == vita_path; }))
            continue;
        if (auto source = firmware_app_source(emuenv, vita_path))
            sources.push_back(std::move(*source));
    }

    std::ranges::sort(sources, [](const AppCacheSource &lhs, const AppCacheSource &rhs) {
        return lhs.app_path < rhs.app_path;
    });

    return sources;
}

static AppEntry read_app_cache_entry(const pugi::xml_node &node, const EmuEnvState &emuenv) {
    AppEntry app;
    app.app_ver = node.attribute("app_ver").as_string();
    app.category = node.attribute("category").as_string();
    app.content_id = node.attribute("content_id").as_string();
    app.addcont = node.attribute("addcont").as_string();
    app.savedata = node.attribute("savedata").as_string();
    app.parental_level = node.attribute("parental_level").as_string();
    app.stitle = node.attribute("stitle").as_string();
    app.title = node.attribute("title").as_string();
    app.title_id = node.attribute("title_id").as_string();
    app.path = node.attribute("path").as_string();
    app.icon_path = get_icon_path(emuenv, app.path, app.title_id);
    return app;
}

static void write_app_cache_entry(pugi::xml_node parent, const AppEntry &app) {
    auto node = parent.append_child("app");
    node.append_attribute("app_ver") = app.app_ver.c_str();
    node.append_attribute("category") = app.category.c_str();
    node.append_attribute("content_id") = app.content_id.c_str();
    node.append_attribute("addcont") = app.addcont.c_str();
    node.append_attribute("savedata") = app.savedata.c_str();
    node.append_attribute("parental_level") = app.parental_level.c_str();
    node.append_attribute("stitle") = app.stitle.c_str();
    node.append_attribute("title") = app.title.c_str();
    node.append_attribute("title_id") = app.title_id.c_str();
    node.append_attribute("path") = app.path.c_str();
}

static std::vector<AppCacheSource> read_app_cache_sources(const pugi::xml_node &sources_node) {
    std::vector<AppCacheSource> sources;
    for (const auto &node : sources_node.children("source")) {
        sources.push_back({
            .app_path = node.attribute("path").as_string(),
            .param_sfo_write_time = node.attribute("param_sfo_write_time").as_llong(-1),
            .icon_write_time = node.attribute("icon_write_time").as_llong(-1),
            .patch_sfo_write_time = node.attribute("patch_sfo_write_time").as_llong(-1),
            .patch_icon_write_time = node.attribute("patch_icon_write_time").as_llong(-1),
        });
    }
    return sources;
}

static bool cache_sources_match(const std::vector<AppCacheSource> &lhs, const std::vector<AppCacheSource> &rhs) {
    if (lhs.size() != rhs.size())
        return false;

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if ((lhs[i].app_path != rhs[i].app_path)
            || (lhs[i].param_sfo_write_time != rhs[i].param_sfo_write_time)
            || (lhs[i].icon_write_time != rhs[i].icon_write_time)
            || (lhs[i].patch_sfo_write_time != rhs[i].patch_sfo_write_time)
            || (lhs[i].patch_icon_write_time != rhs[i].patch_icon_write_time)) {
            return false;
        }
    }

    return true;
}

static bool write_apps_cache_file(const EmuEnvState &emuenv, const std::vector<AppEntry> &apps, const std::vector<AppCacheSource> &sources, uint32_t sys_lang) {
    const fs::path cache_path = get_apps_cache_path(emuenv);
    fs::create_directories(cache_path.parent_path());

    pugi::xml_document doc;
    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "utf-8";

    auto root = doc.append_child("apps-cache");
    root.append_attribute("cache_version") = CACHE_VERSION;
    root.append_attribute("sys_lang") = sys_lang;

    auto sources_node = root.append_child("sources");
    for (const auto &source : sources) {
        auto node = sources_node.append_child("source");
        node.append_attribute("path") = source.app_path.c_str();
        node.append_attribute("param_sfo_write_time") = source.param_sfo_write_time;
        node.append_attribute("icon_write_time") = source.icon_write_time;
        node.append_attribute("patch_sfo_write_time") = source.patch_sfo_write_time;
        node.append_attribute("patch_icon_write_time") = source.patch_icon_write_time;
    }

    auto apps_node = root.append_child("apps");
    for (const auto &app : apps)
        write_app_cache_entry(apps_node, app);

    if (!doc.save_file(cache_path.c_str())) {
        LOG_ERROR("Failed to write apps cache to {}", cache_path);
        return false;
    }

    return true;
}

static std::string resolve_existing_path(const EmuEnvState &emuenv, const fs::path &path) {
    if (path.empty())
        return {};

    if (path.is_absolute()) {
        if (fs::exists(path))
            return path.generic_string();
        return {};
    }

    const std::array<fs::path, 2> roots = {
        emuenv.vita_fs_path,
        emuenv.default_path,
    };

    for (const auto &root : roots) {
        if (root.empty())
            continue;

        const fs::path candidate = root / path;
        if (fs::exists(candidate))
            return candidate.generic_string();
    }

    return {};
}

static std::string get_icon_path(const EmuEnvState &emuenv, const std::string &app_path, const std::string &title_id) {
    const auto cached_icon = vfs::get_app_icon_cache_path(emuenv.vita_fs_path, app_path, title_id);
    if (!cached_icon.empty())
        return cached_icon.generic_string();

    const auto icon = vfs::resolve_app_path(emuenv.vita_fs_path, app_path, "sce_sys/icon0.png", title_id);
    if (fs::exists(icon))
        return icon.generic_string();

    const auto default_rel = fs::path("vs0/data/internal/common/default-icon.png");
    if (const auto resolved = resolve_existing_path(emuenv, default_rel); !resolved.empty())
        return resolved;

    return {};
}

static std::vector<AppTime>::iterator find_app_time(
    AppsListState &state,
    const std::string &user_id,
    const std::string &app_path) {
    auto &v = state.app_times[user_id];
    return std::ranges::find_if(v, [&](const AppTime &t) {
        return t.app_path == app_path;
    });
}

struct SharedAppDataUsage {
    bool verified{};
    bool addcont{};
    bool savedata{};
};

static bool is_safe_app_data_id(const std::string &value) {
    const fs::path path{ value };
    return !value.empty() && value.find('\0') == std::string::npos
        && !path.is_absolute() && !path.has_root_name() && !path.has_root_directory()
        && !path.has_parent_path() && path != "." && path != "..";
}

static std::string safe_app_data_id(const std::string &value, const std::string &fallback) {
    return is_safe_app_data_id(value) ? value : fallback;
}

static SharedAppDataUsage find_shared_app_data(
    EmuEnvState &emuenv,
    const std::string &app_path,
    const std::string &addcont_id,
    const std::string &savedata_id) {
    const auto apps_path{ emuenv.vita_fs_path / "ux0/app" };
    std::error_code err{};
    fs::directory_iterator it{ apps_path, err };
    if (err) {
        LOG_WARN("Unable to verify app-data ownership in '{}': {}", apps_path, err.message());
        return {};
    }

    SharedAppDataUsage usage{};
    while (it != fs::directory_iterator{}) {
        const auto other_app_path{ it->path().filename().string() };
        if (other_app_path != app_path && !other_app_path.ends_with("_dec")) {
            const bool is_app_directory = fs::is_directory(it->path(), err);
            if (err) {
                LOG_WARN("Unable to inspect installed app '{}': {}", it->path(), err.message());
                return {};
            }

            if (is_app_directory) {
                const auto param_path = it->path() / "sce_sys/param.sfo";
                const bool has_param = fs::exists(param_path, err);
                if (err) {
                    LOG_WARN("Unable to inspect '{}': {}; preserving app data.", param_path, err.message());
                    return {};
                }
                if (!has_param) {
                    LOG_DEBUG("Skipping app-data ownership check for '{}' without param.sfo.", it->path());
                } else {
                    vfs::FileBuffer param;
                    if (!vfs::read_file(VitaIoDevice::ux0, param, emuenv.vita_fs_path, fs::path("app") / other_app_path / "sce_sys/param.sfo")) {
                        LOG_WARN("Unable to read '{}'; preserving addcont and savedata because ownership cannot be verified.",
                            param_path);
                        return {};
                    }

                    sfo::SfoAppInfo other_info;
                    if (!sfo::get_param_info(other_info, param, emuenv.cfg.sys_lang)) {
                        LOG_WARN("Malformed param.sfo in '{}', preserving app data.", it->path());
                        return {};
                    }
                    if (!is_safe_app_data_id(other_info.app_addcont) || !is_safe_app_data_id(other_info.app_savedata)) {
                        LOG_WARN("Invalid addcont or savedata metadata in '{}'; preserving app data.", it->path());
                        return {};
                    }

                    usage.addcont = usage.addcont
                        || string_utils::toupper(other_info.app_addcont) == string_utils::toupper(addcont_id);
                    usage.savedata = usage.savedata
                        || string_utils::toupper(other_info.app_savedata) == string_utils::toupper(savedata_id);
                }
            }
        }

        it.increment(err);
        if (err) {
            LOG_WARN("Unable to finish scanning installed apps for shared data: {}", err.message());
            return {};
        }
    }

    usage.verified = true;
    return usage;
}

} // namespace

bool init_apps_list(EmuEnvState &emuenv) {
    if (!load_cached_apps(emuenv))
        return scan_apps(emuenv);
    return true;
}

bool scan_apps(EmuEnvState &emuenv) {
    const auto sources = collect_app_cache_sources(emuenv);
    std::vector<AppEntry> scanned;
    scanned.reserve(sources.size());
    size_t appmeta_existing = 0;
    size_t appmeta_created = 0;
    size_t appmeta_failed = 0;
    size_t shell_cache_refreshed = 0;
    size_t shell_cache_failed = 0;
    size_t appmeta_gp_categories = 0;
    size_t appmeta_addcont = 0;
    size_t appmeta_invalid = 0;
    size_t appmeta_other_sources = 0;
    LOG_INFO("Appmeta refresh: scanning {} application sources", sources.size());
    for (const auto &source : sources) {
        auto entry = read_app_info(emuenv, source.app_path);
        if (!entry.title_id.empty() && entry.path == "ux0:app/" + entry.title_id
            && !entry.category.empty() && entry.category != "-" && entry.category != "ac") {
            if (entry.category.starts_with("gp"))
                ++appmeta_gp_categories;
            const auto metadata = emuenv.vita_fs_path / "ux0" / "appmeta" / entry.title_id;
            bool metadata_ready = false;
            std::error_code error;
            const auto status = fs::symlink_status(metadata, error);
            if (error && error != std::errc::no_such_file_or_directory) {
                ++appmeta_failed;
                LOG_ERROR("Unable to inspect Appmeta for '{}' during app refresh: {}", entry.title_id, error.message());
            } else if (fs::exists(status)) {
                ++appmeta_existing;
                metadata_ready = true;
            } else {
                const auto patch = emuenv.vita_fs_path / "ux0" / "patch" / entry.title_id;
                std::error_code patch_error;
                const auto patch_status = fs::symlink_status(patch, patch_error);
                if (patch_error && patch_error != std::errc::no_such_file_or_directory) {
                    ++appmeta_failed;
                    LOG_ERROR("Unable to inspect patch for '{}' during app refresh: {}", entry.title_id, patch_error.message());
                } else {
                    const auto type = fs::exists(patch_status) ? AppmetaInstallType::Patch : AppmetaInstallType::Application;
                    if (create_appmeta(emuenv.io, emuenv.vita_fs_path, entry.title_id, type)) {
                        ++appmeta_created;
                        metadata_ready = true;
                    } else {
                        ++appmeta_failed;
                        LOG_ERROR("Unable to create missing Appmeta for '{}' during app refresh.", entry.title_id);
                    }
                }
                if (metadata_ready) {
                    if (refresh_appmeta_cache(emuenv.io, emuenv.vita_fs_path, entry.title_id)) {
                        ++shell_cache_refreshed;
                        entry.icon_path = get_icon_path(emuenv, entry.path, entry.title_id);
                    } else {
                        ++shell_cache_failed;
                        LOG_ERROR("Unable to refresh shell Appmeta cache for '{}' during app refresh.", entry.title_id);
                    }
                }
            }
        } else if (!source.app_path.starts_with("ux0:app/")) {
            ++appmeta_other_sources;
        } else if (entry.category == "ac") {
            ++appmeta_addcont;
        } else {
            ++appmeta_invalid;
            LOG_WARN("Appmeta refresh: skipped '{}' (title ID '{}', category '{}')", source.app_path, entry.title_id, entry.category);
        }
        scanned.push_back(std::move(entry));
    }
    LOG_INFO("Appmeta refresh: {} existing, {} created, {} failed (including {} gp-category entries); skipped {} add-on content, {} invalid entries and {} other sources",
        appmeta_existing, appmeta_created, appmeta_failed, appmeta_gp_categories, appmeta_addcont, appmeta_invalid, appmeta_other_sources);
    LOG_INFO("Shell Appmeta cache refresh: {} rebuilt, {} failed", shell_cache_refreshed, shell_cache_failed);

    auto &state = emuenv.app.apps_list;

    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.apps = std::move(scanned);
    }

    save_apps_cache(emuenv);

    return true;
}

bool load_cached_apps(EmuEnvState &emuenv) {
    const fs::path cache_path = get_apps_cache_path(emuenv);
    if (!fs::exists(cache_path))
        return false;

    pugi::xml_document doc;
    if (!doc.load_file(cache_path.c_str())) {
        LOG_WARN("Apps cache at '{}' is unreadable; rebuilding.", cache_path);
        return false;
    }

    const auto root = doc.child("apps-cache");
    if (root.empty()) {
        LOG_WARN("Apps cache at '{}' is malformed; rebuilding.", cache_path);
        return false;
    }

    const uint32_t version = root.attribute("cache_version").as_uint();
    if (version != CACHE_VERSION) {
        LOG_WARN("Apps cache version mismatch (found {}, expected {}); rebuilding.", version, CACHE_VERSION);
        return false;
    }

    const uint32_t cache_lang = root.attribute("sys_lang").as_uint();
    if (cache_lang != static_cast<uint32_t>(emuenv.cfg.sys_lang)) {
        LOG_WARN("Apps cache lang {} differs from config {}; rebuilding.",
            cache_lang, static_cast<uint32_t>(emuenv.cfg.sys_lang));
        return false;
    }

    const auto current_sources = collect_app_cache_sources(emuenv);
    const auto cached_sources = read_app_cache_sources(root.child("sources"));
    if (!cache_sources_match(cached_sources, current_sources)) {
        LOG_INFO("Apps cache source metadata differs from installed content; rebuilding.");
        return false;
    }

    std::vector<AppEntry> loaded;
    for (const auto &node : root.child("apps").children("app"))
        loaded.push_back(read_app_cache_entry(node, emuenv));

    auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);
    state.apps = std::move(loaded);

    return true;
}

void save_apps_cache(EmuEnvState &emuenv) {
    std::vector<AppEntry> apps;
    {
        auto &state = emuenv.app.apps_list;
        std::lock_guard<std::mutex> lock(state.mutex);
        apps = state.apps;
    }

    const auto sources = collect_app_cache_sources(emuenv);
    write_apps_cache_file(emuenv, apps, sources, static_cast<uint32_t>(emuenv.cfg.sys_lang));
}

bool update_firmware_apps(EmuEnvState &emuenv, std::vector<std::string> &added_paths) {
    std::vector<AppCacheSource> firmware_sources;
    for (const auto &path : emuenv.cfg.firmware_apps) {
        if (std::ranges::any_of(firmware_sources, [&](const AppCacheSource &source) { return source.app_path == path; }))
            continue;
        if (auto source = firmware_app_source(emuenv, path))
            firmware_sources.push_back(std::move(*source));
    }

    const auto is_firmware_app = [](const std::string &path) {
        const auto device_id = device::get_device(path);
        if (device_id != VitaIoDevice::vs0 && device_id != VitaIoDevice::pd0)
            return false;
        return fs::path(device::remove_device_from_path(path, device_id)).parent_path() == fs::path("app");
    };
    auto &state = emuenv.app.apps_list;
    const std::lock_guard<std::mutex> lock(state.mutex);
    auto updated = state.apps;
    std::erase_if(updated, [&](const AppEntry &entry) {
        return is_firmware_app(entry.path) && !std::ranges::any_of(firmware_sources, [&](const AppCacheSource &source) {
            return source.app_path == entry.path;
        });
    });
    std::vector<std::string> added;
    for (const auto &source : firmware_sources) {
        if (std::ranges::any_of(updated, [&](const AppEntry &entry) { return entry.path == source.app_path; }))
            continue;
        updated.push_back(read_app_info(emuenv, source.app_path));
        added.push_back(source.app_path);
    }

    const auto cache_path = get_apps_cache_path(emuenv);
    pugi::xml_document doc;
    const auto cache_loaded = doc.load_file(cache_path.c_str());
    const auto root = doc.child("apps-cache");
    bool cache_matches = cache_loaded && !root.empty()
        && root.attribute("cache_version").as_uint() == CACHE_VERSION
        && root.attribute("sys_lang").as_uint() == static_cast<uint32_t>(emuenv.cfg.sys_lang);
    auto sources = read_app_cache_sources(root.child("sources"));
    std::vector<std::string> state_paths;
    std::vector<std::string> source_paths;
    std::vector<std::string> cached_paths;
    for (const auto &entry : state.apps)
        state_paths.push_back(entry.path);
    for (const auto &source : sources)
        source_paths.push_back(source.app_path);
    for (const auto &entry : root.child("apps").children("app"))
        cached_paths.emplace_back(entry.attribute("path").as_string());
    std::ranges::sort(state_paths);
    std::ranges::sort(source_paths);
    std::ranges::sort(cached_paths);
    cache_matches = cache_matches && state_paths == source_paths && state_paths == cached_paths;
    if (cache_matches) {
        std::erase_if(sources, [&](const AppCacheSource &source) {
            return is_firmware_app(source.app_path) && !std::ranges::any_of(firmware_sources, [&](const AppCacheSource &selected) {
                return selected.app_path == source.app_path;
            });
        });
        for (const auto &selected : firmware_sources) {
            const auto existing = std::ranges::find_if(sources, [&](const AppCacheSource &source) { return source.app_path == selected.app_path; });
            if (existing == sources.end()) {
                sources.push_back(selected);
            }
        }
        std::ranges::sort(sources, [](const AppCacheSource &lhs, const AppCacheSource &rhs) { return lhs.app_path < rhs.app_path; });
        if (!write_apps_cache_file(emuenv, updated, sources, static_cast<uint32_t>(emuenv.cfg.sys_lang)))
            return false;
    } else {
        LOG_INFO("Apps cache is missing or stale; leaving it unchanged during firmware app update.");
    }

    state.apps = std::move(updated);
    added_paths = std::move(added);
    return true;
}

AppEntry read_app_info(EmuEnvState &emuenv, const std::string &app_path) {
    const auto full_path = canonical_app_path(app_path);
    const auto app_device = device::get_device(full_path);
    if (full_path.empty() || app_device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot read application info for '{}'.", app_path);
        return {};
    }
    const fs::path relative_path = device::remove_device_from_path(full_path, app_device);
    const auto folder_name = relative_path.filename().string();
    sfo::SfoAppInfo info;
    vfs::FileBuffer param;
    const bool base_read = vfs::read_file(app_device, param, emuenv.vita_fs_path, relative_path / "sce_sys/param.sfo");
    if (!base_read || !sfo::get_param_info(info, param, emuenv.cfg.sys_lang)) {
        LOG_WARN("App list: invalid or unreadable base param.sfo for '{}'", full_path);
        info.app_title_id = folder_name;
        info.app_addcont = folder_name;
        info.app_savedata = folder_name;
        info.app_short_title = folder_name;
        info.app_title = folder_name;
        info.app_version = "0.00";
        info.app_category = "-";
        info.app_parental_level = "0";
    }

    if (app_device != VitaIoDevice::vs0) {
        vfs::FileBuffer patch_param;
        sfo::SfoAppInfo patch_info;
        const auto patch_sfo = fs::path("patch") / info.app_title_id / "sce_sys/param.sfo";
        const auto patch_sfo_path = device::construct_emulated_path(VitaIoDevice::ux0, patch_sfo, emuenv.vita_fs_path);
        std::error_code patch_error;
        const bool patch_exists = fs::exists(patch_sfo_path, patch_error);
        if (patch_error) {
            LOG_ERROR("App list: cannot inspect patch param.sfo for '{}': {}", full_path, patch_error.message());
        } else if (patch_exists) {
            const bool patch_read = vfs::read_file(VitaIoDevice::ux0, patch_param, emuenv.vita_fs_path, patch_sfo);
            if (!patch_read)
                LOG_WARN("App list: unreadable patch param.sfo for '{}'", full_path);
            if (patch_read && sfo::get_param_info(patch_info, patch_param, emuenv.cfg.sys_lang)
                && !patch_info.app_version.empty())
                info.app_version = patch_info.app_version;
        }
    }

    AppEntry app;
    app.app_ver = info.app_version;
    app.category = info.app_category;
    app.content_id = info.app_content_id;
    app.addcont = info.app_addcont;
    app.savedata = info.app_savedata;
    app.parental_level = info.app_parental_level;
    app.stitle = info.app_short_title;
    app.title = info.app_title;
    app.title_id = info.app_title_id;
    app.path = full_path;
    app.icon_path = get_icon_path(emuenv, full_path, info.app_title_id);
    return app;
}

void update_app(EmuEnvState &emuenv, const AppEntry &app) {
    auto &state = emuenv.app.apps_list;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        const auto it = std::ranges::find_if(state.apps, [&](const AppEntry &entry) {
            return entry.path == app.path;
        });
        if (it == state.apps.end())
            state.apps.push_back(app);
        else
            *it = app;
    }
    save_apps_cache(emuenv);
}

void load_app_times(EmuEnvState &emuenv) {
    auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);

    state.app_times.clear();
    const auto time_path{ emuenv.vita_fs_path / "ux0/user/time.xml" };
    if (!fs::exists(time_path))
        return;

    pugi::xml_document doc;
    if (!doc.load_file(time_path.c_str())) {
        LOG_ERROR("time.xml is corrupted at {}; removing.", time_path);
        fs::remove(time_path);
        return;
    }

    const auto time_child = doc.child("time");
    if (time_child.child("user").empty())
        return;

    for (const auto &user : time_child) {
        const std::string user_id = user.attribute("id").as_string();
        for (const auto &app : user) {
            AppTime t;
            t.app_path = canonical_time_app_path(app.text().as_string());
            t.last_time_used = app.attribute("last-time-used").as_llong();
            t.time_used = app.attribute("time-used").as_llong();
            auto &times = state.app_times[user_id];
            const auto existing = std::ranges::find_if(times, [&](const AppTime &entry) { return entry.app_path == t.app_path; });
            if (existing == times.end())
                times.push_back(t);
            else {
                existing->last_time_used = std::max(existing->last_time_used, t.last_time_used);
                existing->time_used += t.time_used;
            }
        }
    }
}

void save_app_times(EmuEnvState &emuenv) {
    auto &state = emuenv.app.apps_list;

    pugi::xml_document doc;
    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "utf-8";

    auto time_child = doc.append_child("time");

    for (auto &[user_id, app_times] : state.app_times) {
        std::ranges::sort(app_times, [](const AppTime &a, const AppTime &b) {
            return a.last_time_used > b.last_time_used;
        });

        auto user_child = time_child.append_child("user");
        user_child.append_attribute("id") = user_id.c_str();

        for (const auto &t : app_times) {
            auto app_child = user_child.append_child("app");
            app_child.append_attribute("last-time-used") = t.last_time_used;
            app_child.append_attribute("time-used") = t.time_used;
            app_child.append_child(pugi::node_pcdata).set_value(t.app_path.c_str());
        }
    }

    const auto time_path{ emuenv.vita_fs_path / "ux0/user/time.xml" };
    if (!doc.save_file(time_path.c_str()))
        LOG_ERROR("Failed to write {}", time_path);
}

void update_last_time_app_used(EmuEnvState &emuenv, const std::string &app_path) {
    auto &state = emuenv.app.apps_list;
    bool first_launch = false;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        const std::string &user_id = emuenv.io.user_id;
        auto it = find_app_time(state, user_id, app_path);

        if (it != state.app_times[user_id].end()) {
            it->last_time_used = std::time(nullptr);
        } else {
            first_launch = true;
            state.app_times[user_id].push_back({ app_path, std::time(nullptr), 0 });
        }

        save_app_times(emuenv);
    }

    if (first_launch && !app_path.starts_with("vs0") && app_path.find("PCS") != std::string::npos) {
        ActivityPostStatus status{ .type = "game_start", .title_id = emuenv.io.title_id };
        v3kn::create_and_post_activity_status(emuenv, status);
    }
}

void update_app_time_used(EmuEnvState &emuenv, const std::string &app_path) {
    auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);

    const std::string &user_id = emuenv.io.user_id;
    auto it = find_app_time(state, user_id, app_path);

    if (it == state.app_times[user_id].end()) {
        LOG_WARN("No time record for '{}'; skipping.", app_path);
        return;
    }

    const auto now = std::time(nullptr);
    it->time_used += now - it->last_time_used;
    it->last_time_used = now;

    save_app_times(emuenv);
}

void reset_last_time_app_used(EmuEnvState &emuenv, const std::string &app_path) {
    auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);

    const std::string &user_id = emuenv.io.user_id;
    auto it = find_app_time(state, user_id, app_path);

    if (it != state.app_times[user_id].end())
        it->last_time_used = 0;
    else
        state.app_times[user_id].push_back({ app_path, 0, 0 });

    save_app_times(emuenv);
}

bool delete_app(EmuEnvState &emuenv, const std::string &app_path) {
    const auto found = get_app(emuenv, app_path);
    if (!found) {
        LOG_WARN("'{}' not found in apps list.", app_path);
        return false;
    }
    const AppEntry &app_entry = *found;
    const auto app_device = device::get_device(app_entry.path);
    const fs::path relative_path = device::remove_device_from_path(app_entry.path, app_device);
    const auto folder_name = relative_path.filename().string();
    if (app_device != VitaIoDevice::ux0 || relative_path.parent_path() != fs::path("app") || !is_safe_app_data_id(folder_name)) {
        LOG_ERROR("Refusing to delete application outside ux0:app: '{}'.", app_entry.path);
        return false;
    }

    const bool addcont_id_valid = is_safe_app_data_id(app_entry.addcont);
    const bool savedata_id_valid = is_safe_app_data_id(app_entry.savedata);
    const auto addcont_id{ safe_app_data_id(app_entry.addcont, folder_name) };
    const auto savedata_id{ safe_app_data_id(app_entry.savedata, folder_name) };
    const auto title_id{ safe_app_data_id(app_entry.title_id, folder_name) };
    auto shared_data{ find_shared_app_data(emuenv, folder_name, addcont_id, savedata_id) };
    bool shared_title = false;
    bool shared_config = false;
    for (const auto &other : get_apps(emuenv)) {
        if (other.path == app_entry.path)
            continue;
        shared_title |= other.title_id == title_id;
        shared_config |= fs::path(other.path).filename() == folder_name;
        if (!other.path.starts_with("ux0:app/")) {
            if (!is_safe_app_data_id(other.addcont) || !is_safe_app_data_id(other.savedata))
                shared_data.verified = false;
            shared_data.addcont |= string_utils::toupper(other.addcont) == string_utils::toupper(addcont_id);
            shared_data.savedata |= string_utils::toupper(other.savedata) == string_utils::toupper(savedata_id);
        }
    }
    if (!shared_data.verified)
        LOG_WARN("Keeping addcont and savedata for '{}' because ownership could not be verified.", app_path);

    try {
        fs::remove_all(emuenv.vita_fs_path / "ux0/app" / folder_name);

        const auto appmeta{ emuenv.vita_fs_path / "ux0/appmeta" / title_id };
        if (fs::exists(appmeta))
            fs::remove_all(appmeta);

        const auto custom_cfg{ emuenv.config_path / "config" / fmt::format("config_{}.xml", folder_name) };
        if (!shared_config && fs::exists(custom_cfg))
            fs::remove_all(custom_cfg);

        const auto addcont{ emuenv.vita_fs_path / "ux0/addcont" / addcont_id };
        if (shared_data.verified && addcont_id_valid && !shared_data.addcont && fs::exists(addcont))
            fs::remove_all(addcont);
        else if (!addcont_id_valid)
            LOG_WARN("Keeping addcont because '{}' contains an invalid app-data ID.", app_path);
        else if (shared_data.addcont)
            LOG_WARN("Keeping addcont '{}' because another installed app uses it.", addcont_id);

        const auto license{ emuenv.vita_fs_path / "ux0/license" / title_id };
        if (!shared_title && fs::exists(license))
            fs::remove_all(license);

        const auto patch{ emuenv.vita_fs_path / "ux0/patch" / title_id };
        if (!shared_title && fs::exists(patch))
            fs::remove_all(patch);

        const auto savedata{ emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "savedata" / savedata_id };
        if (shared_data.verified && savedata_id_valid && !shared_data.savedata && fs::exists(savedata))
            fs::remove_all(savedata);
        else if (!savedata_id_valid)
            LOG_WARN("Keeping savedata because '{}' contains an invalid app-data ID.", app_path);
        else if (shared_data.savedata)
            LOG_WARN("Keeping savedata '{}' because another installed app uses it.", savedata_id);

        const auto shader_cache{ emuenv.cache_path / "shaders" / title_id };
        if (!shared_title && fs::exists(shader_cache))
            fs::remove_all(shader_cache);

        const auto shader_log{ emuenv.cache_path / "shaderlog" / title_id };
        if (!shared_title && fs::exists(shader_log))
            fs::remove_all(shader_log);

        const auto shader_log2{ emuenv.log_path / "shaderlog" / title_id };
        if (!shared_title && fs::exists(shader_log2))
            fs::remove_all(shader_log2);

        const auto export_tex{ emuenv.shared_path / "textures/export" / title_id };
        if (!shared_title && fs::exists(export_tex))
            fs::remove_all(export_tex);

        const auto import_tex{ emuenv.shared_path / "textures/import" / title_id };
        if (!shared_title && fs::exists(import_tex))
            fs::remove_all(import_tex);

        LOG_INFO("App successfully deleted '{}' [{}].", app_entry.title_id, app_entry.title);
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to delete '{}' [{}]: {}", app_entry.title_id, app_entry.title, e.what());
        return false;
    }

    {
        auto &state = emuenv.app.apps_list;
        std::lock_guard<std::mutex> lock(state.mutex);

        std::erase_if(state.apps, [&](const AppEntry &app) { return app.path == app_entry.path; });

        for (auto &[user_id, times] : state.app_times) {
            std::erase_if(times, [&](const AppTime &t) { return t.app_path == app_entry.path; });
        }
        save_app_times(emuenv);
    }

    save_apps_cache(emuenv);
    return true;
}

std::vector<AppEntry> get_apps(const EmuEnvState &emuenv) {
    const auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.apps;
}

std::optional<AppEntry> get_app(const EmuEnvState &emuenv, const std::string &app_path) {
    const auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto full_path = canonical_app_path(app_path);
    auto it = std::ranges::find_if(state.apps, [&](const AppEntry &app) {
        return app.path == full_path;
    });
    if (it == state.apps.end() && app_path.find(':') == std::string::npos)
        it = std::ranges::find_if(state.apps, [&](const AppEntry &app) {
            return app.path.starts_with("ux0:app/") && app.title_id == app_path;
        });
    if (it == state.apps.end())
        return std::nullopt;
    return *it;
}

void sort_apps(EmuEnvState &emuenv, const std::function<bool(const AppEntry &, const AppEntry &)> &compare) {
    auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);
    std::sort(state.apps.begin(), state.apps.end(), compare);
}

std::map<std::string, AppTime> get_user_app_times(const EmuEnvState &emuenv) {
    const auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);
    std::map<std::string, AppTime> result;
    const auto it = state.app_times.find(emuenv.io.user_id);
    if (it != state.app_times.end()) {
        for (const auto &t : it->second)
            result[t.app_path] = t;
    }
    return result;
}

bool set_app_info(EmuEnvState &emuenv, const std::string &app_path) {
    if (app_path == "vs0:vsh/shell") {
        if (!fs::is_regular_file(emuenv.vita_fs_path / "vs0/vsh/shell/shell.self")) {
            LOG_ERROR("Shell executable not found for '{}'.", app_path);
            return false;
        }
        emuenv.io.app_path = app_path;
        emuenv.io.title_id = "NPXS19999";
        emuenv.io.addcont = emuenv.io.title_id;
        emuenv.io.content_id.clear();
        emuenv.io.savedata = emuenv.io.title_id;
        emuenv.current_app_title = "PlayStation Vita OS";
        emuenv.app_info.app_version = "1.00";
        emuenv.app_info.app_category = "gda";
        emuenv.app_info.app_short_title = "PS Vita OS";
        return true;
    }

    auto it = get_app(emuenv, app_path);
    if (!it && (app_path.starts_with("vs0:app/") || app_path.starts_with("pd0:app/")) && firmware_app_source(emuenv, app_path))
        it = read_app_info(emuenv, app_path);
    if (!it) {
        LOG_ERROR("{} not found in apps list.", app_path);
        return false;
    }

    emuenv.io.app_path = it->path;
    emuenv.io.title_id = it->title_id;
    emuenv.io.addcont = it->addcont;
    emuenv.io.content_id = it->content_id;
    emuenv.io.savedata = it->savedata;
    emuenv.current_app_title = it->title;
    emuenv.app_info.app_version = it->app_ver;
    emuenv.app_info.app_category = it->category;
    emuenv.app_info.app_short_title = it->stitle;

    return true;
}

} // namespace app
