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
#include <io/state.h>
#include <packages/sfo.h>
#include <util/fs.h>
#include <util/log.h>

#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <vector>

static constexpr uint32_t CACHE_VERSION = 1;

namespace app {

namespace {

struct AppCacheSource {
    std::string title_id;
    int64_t param_sfo_write_time{ -1 };
    int64_t icon_write_time{ -1 };
};

static std::string get_icon_path(const EmuEnvState &emuenv, const std::string &title_id);

static fs::path get_apps_cache_path(const EmuEnvState &emuenv) {
    return emuenv.config_path / "gui-configs" / "apps-cache.xml";
}

static int64_t get_path_write_time(const fs::path &path) {
    if (!fs::exists(path))
        return -1;

    try {
        return static_cast<int64_t>(fs::last_write_time(path));
    } catch (const std::exception &e) {
        LOG_WARN("Failed to read timestamp for '{}': {}", path, e.what());
        return -1;
    }
}

static std::vector<AppCacheSource> collect_app_cache_sources(const EmuEnvState &emuenv) {
    const fs::path app_path{ emuenv.vita_fs_path / "ux0/app" };
    if (!fs::exists(app_path))
        return {};

    std::vector<AppCacheSource> sources;
    for (const auto &entry : fs::directory_iterator(app_path)) {
        if (entry.path().empty() || !fs::is_directory(entry.path()) || entry.path().filename_is_dot() || entry.path().filename_is_dot_dot())
            continue;
        if (entry.path().filename().string().ends_with("_dec"))
            continue;

        const auto title_id = entry.path().filename().generic_string();
        sources.push_back({
            .title_id = title_id,
            .param_sfo_write_time = get_path_write_time(entry.path() / "sce_sys/param.sfo"),
            .icon_write_time = get_path_write_time(entry.path() / "sce_sys/icon0.png"),
        });
    }

    std::sort(sources.begin(), sources.end(), [](const AppCacheSource &lhs, const AppCacheSource &rhs) {
        return lhs.title_id < rhs.title_id;
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
    app.icon_path = get_icon_path(emuenv, app.title_id);
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
            .title_id = node.attribute("title_id").as_string(),
            .param_sfo_write_time = node.attribute("param_sfo_write_time").as_llong(-1),
            .icon_write_time = node.attribute("icon_write_time").as_llong(-1),
        });
    }
    return sources;
}

static bool cache_sources_match(const std::vector<AppCacheSource> &lhs, const std::vector<AppCacheSource> &rhs) {
    if (lhs.size() != rhs.size())
        return false;

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if ((lhs[i].title_id != rhs[i].title_id)
            || (lhs[i].param_sfo_write_time != rhs[i].param_sfo_write_time)
            || (lhs[i].icon_write_time != rhs[i].icon_write_time)) {
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
        node.append_attribute("title_id") = source.title_id.c_str();
        node.append_attribute("param_sfo_write_time") = source.param_sfo_write_time;
        node.append_attribute("icon_write_time") = source.icon_write_time;
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

static std::string get_icon_path(const EmuEnvState &emuenv, const std::string &title_id) {
    const auto rel = fs::path("ux0/app") / title_id / "sce_sys/icon0.png";
    if (const auto resolved = resolve_existing_path(emuenv, rel); !resolved.empty())
        return resolved;

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
    return std::find_if(v.begin(), v.end(), [&](const AppTime &t) {
        return t.app_path == app_path;
    });
}

} // namespace

bool init_apps_list(EmuEnvState &emuenv) {
    if (!load_cached_apps(emuenv))
        return scan_apps(emuenv);
    return true;
}

bool scan_apps(EmuEnvState &emuenv) {
    const fs::path app_path{ emuenv.vita_fs_path / "ux0/app" };
    if (!fs::exists(app_path))
        return false;

    const auto sources = collect_app_cache_sources(emuenv);
    std::vector<AppEntry> scanned;
    scanned.reserve(sources.size());
    for (const auto &source : sources)
        scanned.push_back(read_app_info(emuenv, source.title_id));

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

AppEntry read_app_info(EmuEnvState &emuenv, const std::string &title_id) {
    sfo::SfoAppInfo info;
    vfs::FileBuffer param;
    if (vfs::read_app_file(param, emuenv.vita_fs_path, title_id, "sce_sys/param.sfo")) {
        sfo::get_param_info(info, param, emuenv.cfg.sys_lang);
    } else {
        info.app_title_id = title_id;
        info.app_addcont = title_id;
        info.app_savedata = title_id;
        info.app_short_title = title_id;
        info.app_title = title_id;
        info.app_version = "N/A";
        info.app_category = "N/A";
        info.app_parental_level = "N/A";
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
    app.path = title_id;
    app.icon_path = get_icon_path(emuenv, title_id);
    return app;
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
            t.app_path = app.text().as_string();
            t.last_time_used = app.attribute("last-time-used").as_llong();
            t.time_used = app.attribute("time-used").as_llong();
            state.app_times[user_id].push_back(t);
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
        std::sort(app_times.begin(), app_times.end(), [](const AppTime &a, const AppTime &b) {
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
    std::lock_guard<std::mutex> lock(state.mutex);

    const std::string &user_id = emuenv.io.user_id;
    auto it = find_app_time(state, user_id, app_path);

    if (it != state.app_times[user_id].end()) {
        it->last_time_used = std::time(nullptr);
    } else {
        state.app_times[user_id].push_back({ app_path, std::time(nullptr), 0 });
    }

    save_app_times(emuenv);
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

void delete_app(EmuEnvState &emuenv, const std::string &app_path) {
    AppEntry app_entry;
    {
        auto &state = emuenv.app.apps_list;
        std::lock_guard<std::mutex> lock(state.mutex);
        const auto it = std::find_if(state.apps.begin(), state.apps.end(),
            [&](const AppEntry &app) { return app.path == app_path; });
        if (it == state.apps.end()) {
            LOG_WARN("'{}' not found in apps list.", app_path);
            return;
        }
        app_entry = *it;
    }

    try {
        fs::remove_all(emuenv.vita_fs_path / "ux0/app" / app_path);

        const auto custom_cfg{ emuenv.config_path / "config" / fmt::format("config_{}.xml", app_path) };
        if (fs::exists(custom_cfg))
            fs::remove_all(custom_cfg);

        const auto addcont{ emuenv.vita_fs_path / "ux0/addcont" / app_entry.addcont };
        if (fs::exists(addcont))
            fs::remove_all(addcont);

        const auto license{ emuenv.vita_fs_path / "ux0/license" / app_entry.title_id };
        if (fs::exists(license))
            fs::remove_all(license);

        const auto patch{ emuenv.vita_fs_path / "ux0/patch" / app_entry.title_id };
        if (fs::exists(patch))
            fs::remove_all(patch);

        const auto savedata{ emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "savedata" / app_entry.savedata };
        if (fs::exists(savedata))
            fs::remove_all(savedata);

        const auto shader_cache{ emuenv.cache_path / "shaders" / app_entry.title_id };
        if (fs::exists(shader_cache))
            fs::remove_all(shader_cache);

        const auto shader_log{ emuenv.cache_path / "shaderlog" / app_entry.title_id };
        if (fs::exists(shader_log))
            fs::remove_all(shader_log);

        const auto shader_log2{ emuenv.log_path / "shaderlog" / app_entry.title_id };
        if (fs::exists(shader_log2))
            fs::remove_all(shader_log2);

        const auto export_tex{ emuenv.shared_path / "textures/export" / app_entry.title_id };
        if (fs::exists(export_tex))
            fs::remove_all(export_tex);

        const auto import_tex{ emuenv.shared_path / "textures/import" / app_entry.title_id };
        if (fs::exists(import_tex))
            fs::remove_all(import_tex);

        LOG_INFO("App successfully deleted '{}' [{}].", app_entry.title_id, app_entry.title);
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to delete '{}' [{}]: {}", app_entry.title_id, app_entry.title, e.what());
    }

    {
        auto &state = emuenv.app.apps_list;
        std::lock_guard<std::mutex> lock(state.mutex);

        state.apps.erase(
            std::remove_if(state.apps.begin(), state.apps.end(),
                [&](const AppEntry &app) { return app.path == app_path; }),
            state.apps.end());

        for (auto &[user_id, times] : state.app_times) {
            times.erase(
                std::remove_if(times.begin(), times.end(),
                    [&](const AppTime &t) { return t.app_path == app_path; }),
                times.end());
        }

        save_app_times(emuenv);
    }

    save_apps_cache(emuenv);
}

std::vector<AppEntry> get_apps(const EmuEnvState &emuenv) {
    const auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.apps;
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
    const auto &state = emuenv.app.apps_list;
    std::lock_guard<std::mutex> lock(state.mutex);

    const auto it = std::find_if(state.apps.begin(), state.apps.end(), [&](const AppEntry &app) { return app.path == app_path; });

    if (it == state.apps.end()) {
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
