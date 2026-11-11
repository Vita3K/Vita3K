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

#include <io/device.h>
#include <io/functions.h>
#include <io/io.h>
#include <io/state.h>
#include <io/types.h>
#include <io/util.h>
#include <io/vfs.h>

#include <kernel/state.h>
#include <kernel/sync_primitives.h>
#include <mem/ptr.h>
#include <pfsmgr/functions.h>
#include <rtc/rtc.h>
#include <util/log.h>
#include <util/preprocessor.h>
#include <util/string_utils.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <system_error>

#if defined(__aarch64__) && defined(__APPLE__)
#define stat64 stat
#endif

// ****************************
// * Utility functions *
// ****************************

static int io_error_impl(const int retval, const char *export_name, const char *func_name) {
    LOG_WARN("{} ({}) returned {}", func_name, export_name, log_hex(retval));
    return retval;
}

static bool overlaps_savedata_mount(IOState &io, const fs::path &path) {
    std::lock_guard lock(io.appmeta_mutex);
    if (io.savedata_pfs_mount.empty())
        return false;
    const auto mount = io.pfs_mounts.get_directory(io.savedata_pfs_mount);
    if (!mount) {
        LOG_ERROR("Missing experimental savedata mount");
        return true;
    }
    std::error_code error;
    const auto target = fs::weakly_canonical(path, error);
    if (error || target.empty()) {
        LOG_ERROR("Cannot check protected savedata path '{}': {}", path, error.message());
        return true;
    }
    auto root_part = mount->canonical_source.begin();
    auto target_part = target.begin();
    while (root_part != mount->canonical_source.end() && target_part != target.end()) {
#ifdef _WIN32
        if (string_utils::tolower(fs_utils::path_to_utf8(*root_part)) != string_utils::tolower(fs_utils::path_to_utf8(*target_part)))
#else
        if (*root_part != *target_part)
#endif
            return false;
        ++root_part;
        ++target_part;
    }
    return true;
}

#define IO_ERROR(retval) io_error_impl(retval, export_name, __func__)
#define IO_ERROR_UNK() IO_ERROR(-1)

constexpr bool log_file_op = true;
constexpr bool log_file_read = false;
constexpr bool log_file_seek = false;
constexpr bool log_file_stat = false;

AsyncIoRequest::~AsyncIoRequest() {
    if (worker.joinable())
        worker.join();
    if (cleanup)
        cleanup();
}

static fs::path expand_unresolved_path(IOState &io, const char *path, const fs::path &vita_fs_path);
static fs::path find_host_path(IOState &io, const fs::path &path);
static fs::path appmeta_host_path(IOState &io, const char *path);

using fs_utils::is_plain_component;
using pfsmgr::is_mount_name;

static bool is_appmeta_mount_name(const std::string &name) {
    return is_mount_name(name, "ad");
}

static bool is_appmeta_path(std::string_view path) {
    const auto colon = path.find(':');
    return (colon == 14 && (path.starts_with("ad") || path.starts_with("gp") || path.starts_with("tm")))
        || (colon == 16 && path.starts_with("PD"));
}

static std::string resolve_mount_alias(const IOState &io, const std::string &key) {
    std::optional<pfsmgr::MountType> type;
    if (is_mount_name(key, "ad"))
        type = pfsmgr::MountType::AppMeta;
    else if (is_mount_name(key, "gp"))
        type = pfsmgr::MountType::GameData;
    else if (is_mount_name(key, "tm"))
        type = pfsmgr::MountType::ThemeData;
    if (type) {
        const auto alias = io.virtual_pfs_mounts.find({ *type, key });
        if (alias != io.virtual_pfs_mounts.end())
            return alias->second;
    }
    return key;
}

static std::string mounted_path_key(const IOState &io, const std::string &path) {
    const auto colon = path.find(':');
    if (colon == std::string::npos)
        return {};
    auto key = path.substr(0, colon + 1);
    if (key == io.addcont_pfs_mount && path.size() > colon + 2 && path[colon + 1] == '/') {
        const auto end = path.find('/', colon + 2);
        key = path.substr(0, end);
    }
    return resolve_mount_alias(io, key);
}

static std::vector<fs::path> appmeta_host_paths(IOState &io, const char *path) {
    const std::string input(path);
    const auto colon = input.find(':');
    if (colon == std::string::npos)
        return {};
    const auto name = input.substr(0, colon + 1);
    if (!is_appmeta_mount_name(name) && !is_mount_name(name, "gp") && !is_mount_name(name, "tm") && !is_mount_name(name, "PD"))
        return {};
    std::lock_guard lock(io.appmeta_mutex);
    const auto key = mounted_path_key(io, input);
    if (key == io.savedata_pfs_mount && io.savedata_access && io.savedata_access->needs_recovery.load()) {
        LOG_ERROR("Protected savedata requires recovery before access");
        return {};
    }
    const auto relative_start = key.starts_with(io.addcont_pfs_mount) && !io.addcont_pfs_mount.empty() ? key.size() : colon + 1;
    auto relative = input.substr(relative_start);
    if (relative.find_first_of("\\:") != std::string::npos)
        return {};
    if (!relative.empty() && relative.front() == '/')
        relative.erase(0, 1);
    if (!relative.empty() && relative.front() == '/')
        return {};
    if (!relative.empty() && relative.back() == '/')
        relative.pop_back();
    if (relative.find("//") != std::string::npos)
        return {};
    const auto mounted = io.pfs_mounts.get_directory(key);
    if (!mounted)
        return {};
    const auto &mount = *mounted;
    std::error_code ec;
    if (!is_plain_component(mount.source.parent_path().parent_path()) || !is_plain_component(mount.source.parent_path()) || !is_plain_component(mount.source) || fs::canonical(mount.source, ec) != mount.canonical_source || ec)
        return {};
    if (!mount.patch.empty() && (!is_plain_component(mount.patch.parent_path()) || !is_plain_component(mount.patch) || fs::canonical(mount.patch, ec) != mount.canonical_patch || ec))
        return {};
    std::vector<fs::path> results;
    for (const auto &[root, id] : { std::pair{ mount.patch, mount.layers.patch }, std::pair{ mount.source, mount.layers.base } }) {
        if (root.empty())
            continue;
        const auto layer = io.pfs_mounts.get(id);
        if (id != pfsmgr::no_mount && !layer) {
            LOG_ERROR("Missing PFS mount for '{}'", path);
            return {};
        }
        try {
            if (layer && !relative.empty() && !layer->find(fs_utils::utf8_to_path(relative)))
                continue;
            if (layer && layer->mode() == pfsmgr::Mode::Mode5) {
                results.push_back(layer->savedata_host_path(fs_utils::utf8_to_path(relative)));
                break;
            }
        } catch (const std::runtime_error &error) {
            LOG_ERROR("Cannot resolve PFS path '{}': {}", path, error.what());
            return {};
        }
        auto result = root;
        bool missing = false;
        for (const auto &part : fs_utils::utf8_to_path(relative)) {
            const auto component = fs_utils::path_to_utf8(part);
            if (component.empty() || component == "." || component == ".." || component.back() == '.' || component.back() == ' ' || std::any_of(component.begin(), component.end(), [](unsigned char c) { return c < 0x20; }))
                return {};
            result /= part;
            auto status = fs::symlink_status(result, ec);
            if (!layer && io.case_isens_find_enabled && (ec == std::errc::no_such_file_or_directory || (!ec && !fs::exists(status)))) {
                ec.clear();
                result = find_host_path(io, result);
                status = fs::symlink_status(result, ec);
            }
            if (ec == std::errc::no_such_file_or_directory || (!ec && !fs::exists(status))) {
                ec.clear();
                missing = true;
                break;
            }
            if (ec || !is_plain_component(result))
                return {};
        }
        if (missing && layer)
            return {};
        if (!missing) {
            results.push_back(result);
            if (!fs::is_directory(result, ec) || ec)
                break;
        }
    }
    return results;
}

static fs::path appmeta_host_path(IOState &io, const char *path) {
    const auto paths = appmeta_host_paths(io, path);
    return paths.empty() ? fs::path{} : paths.front();
}

static bool game_data_path_is_tracked(IOState &io, const std::string &path) {
    const auto colon = path.find(':');
    if (colon == std::string::npos || (!is_mount_name(path.substr(0, colon + 1), "gp") && !is_mount_name(path.substr(0, colon + 1), "tm") && !is_mount_name(path.substr(0, colon + 1), "PD")))
        return false;
    std::lock_guard lock(io.appmeta_mutex);
    const auto key = mounted_path_key(io, path);
    const auto relative_start = key.starts_with(io.addcont_pfs_mount) && !io.addcont_pfs_mount.empty() ? key.size() : colon + 1;
    auto relative = path.substr(relative_start);
    if (!relative.empty() && relative.front() == '/')
        relative.erase(0, 1);
    if (relative.empty())
        return false;
    const auto mount = io.pfs_mounts.get_directory(key);
    if (!mount || mount->layers.base == pfsmgr::no_mount)
        return false;
    const auto layer = io.pfs_mounts.get(mount->layers.base);
    if (!layer) {
        LOG_ERROR("Missing PFS mount for '{}'", path);
        return true;
    }
    try {
        return layer->find(fs_utils::utf8_to_path(relative)) != nullptr;
    } catch (const std::runtime_error &error) {
        LOG_ERROR("Cannot inspect mounted PFS path '{}': {}", path, error.what());
        return true;
    }
}

struct ResolvedGameLayer {
    std::shared_ptr<const pfsmgr::Layer> layer;
    fs::path relative;
    std::shared_ptr<void> lease;
    pfsmgr::MountScope scope = pfsmgr::MountScope::Application;
};

static std::optional<ResolvedGameLayer> game_data_layer(IOState &io, const char *path, const fs::path &host) {
    const std::string input(path);
    const auto colon = input.find(':');
    if (colon == std::string::npos || (!is_mount_name(input.substr(0, colon + 1), "ad") && !is_mount_name(input.substr(0, colon + 1), "gp") && !is_mount_name(input.substr(0, colon + 1), "tm") && !is_mount_name(input.substr(0, colon + 1), "PD")))
        return ResolvedGameLayer{};
    std::lock_guard lock(io.appmeta_mutex);
    const auto mounted = io.pfs_mounts.get_directory(mounted_path_key(io, input));
    if (!mounted)
        return {};
    const auto &mount = *mounted;
    auto id = mount.layers.base;
    auto relative = host.lexically_relative(mount.source);
    if (!mount.patch.empty()) {
        const auto patch_relative = host.lexically_relative(mount.patch);
        if (!patch_relative.empty() && *patch_relative.begin() != "..") {
            id = mount.layers.patch;
            relative = patch_relative;
        }
    }
    const auto layer = io.pfs_mounts.get(id);
    if (id != pfsmgr::no_mount && !layer) {
        LOG_ERROR("Missing PFS mount for '{}'", path);
        return {};
    }
    return ResolvedGameLayer{ layer, relative, mount.lease, mount.scope };
}

namespace vfs {

bool read_file(const VitaIoDevice device, FileBuffer &buf, const fs::path &vita_fs_path, const fs::path &vfs_file_path) {
    const auto host_file_path = device::construct_emulated_path(device, vfs_file_path, vita_fs_path);
    return fs_utils::read_data(host_file_path, buf);
}

fs::path get_app_base_path(const fs::path &vita_fs_path, const std::string &app_path) {
    IOState io{};
    io.app_path = app_path;
    init_device_paths(io, vita_fs_path);
    if (io.device_paths.app0_device == VitaIoDevice::_INVALID || io.device_paths.app0.empty()) {
        LOG_ERROR("Cannot resolve application directory for '{}'.", app_path);
        return {};
    }
    return device::construct_emulated_path(io.device_paths.app0_device, io.device_paths.app0, vita_fs_path);
}

static std::optional<fs::path> appmeta_resource_path(const fs::path &vita_fs_path, const std::string &app_path,
    const fs::path &relative, const std::string &title_id) {
    if (title_id.empty() || app_path != "ux0:app/" + title_id || relative.has_root_path())
        return std::nullopt;
    auto part = relative.begin();
    if (part == relative.end() || *part++ != "sce_sys" || part == relative.end())
        return std::nullopt;
    const auto category = *part++;
    if (category == "icon0.png" || category == "pic0.png" || category == "param.sfo" || category == "param2.sfo") {
        if (part != relative.end())
            return std::nullopt;
    } else if (category == "retail") {
        if (part == relative.end() || *part++ != "livearea")
            return std::nullopt;
    } else if (category != "livearea") {
        return std::nullopt;
    }
    if (title_id.size() > 32 || !std::all_of(title_id.begin(), title_id.end(), [](unsigned char ch) {
            return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
        }))
        return fs::path{};
    const auto ux0 = vita_fs_path / "ux0";
    const auto parent = ux0 / "appmeta";
    const auto root = parent / title_id;
    std::error_code error;
    const auto status = fs::symlink_status(root, error);
    if (error == std::errc::no_such_file_or_directory || (!error && !fs::exists(status)))
        return std::nullopt;
    if (error || !is_plain_component(ux0) || !is_plain_component(parent) || !is_plain_component(root) || !fs::is_directory(status)) {
        LOG_ERROR("Cannot resolve invalid Appmeta directory for '{}'", title_id);
        return fs::path{};
    }
    fs::path result = root;
    for (auto component = ++relative.begin(); component != relative.end(); ++component) {
        if (*component == "." || *component == ".." || component->empty()) {
            LOG_ERROR("Invalid Appmeta resource path for '{}'", title_id);
            return fs::path{};
        }
        result /= *component;
        const auto component_status = fs::symlink_status(result, error);
        if (error == std::errc::no_such_file_or_directory || (!error && !fs::exists(component_status))) {
            error.clear();
            continue;
        }
        if (error || !is_plain_component(result)) {
            LOG_ERROR("Cannot resolve Appmeta resource for '{}'", title_id);
            return fs::path{};
        }
    }
    return result;
}

fs::path resolve_app_path(const fs::path &vita_fs_path, const std::string &app_path, const fs::path &vfs_file_path, const std::string &title_id) {
    IOState io{};
    io.app_path = app_path;
    io.title_id = title_id.empty() ? fs::path(app_path).filename().string() : title_id;
    if (const auto appmeta = appmeta_resource_path(vita_fs_path, app_path, vfs_file_path, io.title_id)) {
        bool protected_appmeta = false;
        if (!pfsmgr::has_appmeta_pfs(vita_fs_path / "ux0" / "appmeta" / io.title_id, protected_appmeta))
            return {};
        if (protected_appmeta) {
            LOG_WARN("Cannot expose protected appmeta as a host path for '{}'", io.title_id);
            return {};
        }
        return *appmeta;
    }
#ifndef _WIN32
    io.case_isens_find_enabled = true;
#endif
    init_device_paths(io, vita_fs_path);
    const auto path = "app0:/" + vfs_file_path.generic_string();
    return expand_path(io, path.c_str(), vita_fs_path);
}

bool read_app_file(FileBuffer &buf, const fs::path &vita_fs_path, const std::string &app_path, const fs::path &vfs_file_path, const std::string &title_id) {
    const auto id = title_id.empty() ? fs::path(app_path).filename().string() : title_id;
    if (const auto appmeta = appmeta_resource_path(vita_fs_path, app_path, vfs_file_path, id)) {
        if (appmeta->empty())
            return false;
        const auto root = vita_fs_path / "ux0" / "appmeta" / id;
        bool protected_appmeta = false;
        if (!pfsmgr::has_appmeta_pfs(root, protected_appmeta))
            return false;
        try {
            if (protected_appmeta) {
                const pfsmgr::Layer layer(root);
                auto data = layer.read(appmeta->lexically_relative(root));
                if (!data) {
                    LOG_WARN("Missing protected appmeta resource '{}' for '{}'", vfs_file_path, id);
                    return false;
                }
                buf = std::move(*data);
                return true;
            }
        } catch (const std::runtime_error &error) {
            LOG_ERROR("Cannot read protected appmeta resource '{}' for '{}': {}", vfs_file_path, id, error.what());
            return false;
        }
    }
    return fs_utils::read_data(resolve_app_path(vita_fs_path, app_path, vfs_file_path, title_id), buf);
}

bool app_dir_has_entries(const fs::path &vita_fs_path, const std::string &app_path, const fs::path &vfs_dir_path, const std::string &title_id) {
    IOState io{};
    io.app_path = app_path;
    io.title_id = title_id.empty() ? fs::path(app_path).filename().string() : title_id;
#ifndef _WIN32
    io.case_isens_find_enabled = true;
#endif
    init_device_paths(io, vita_fs_path);
    const auto path = "app0:/" + vfs_dir_path.generic_string();
    for (const auto &candidate : resolve_overlay_paths(io, path.c_str(), vita_fs_path, false, 0, 0xFF, true)) {
        const auto host = find_host_path(io, expand_unresolved_path(io, candidate.c_str(), vita_fs_path));
        if (!fs::exists(host))
            continue;
        if (!fs::is_directory(host))
            return false;
        if (!fs::is_empty(host))
            return true;
    }
    return false;
}

SceSize get_directory_used_size(const VitaIoDevice device, const std::string &vfs_path, const fs::path &vita_fs_path) {
    const auto emuenv_path = device::construct_emulated_path(device, vfs_path, vita_fs_path);

    SceSize total_size = 0;
    for (const auto &entry : fs::recursive_directory_iterator(emuenv_path)) {
        if (fs::is_regular_file(entry.path()))
            total_size += fs::file_size(entry.path());
    }

    return total_size;
}

} // namespace vfs

// ****************************
// * End utility functions *
// ****************************

static bool is_valid_output_path(const VitaIoDevice device) {
    return !(device == VitaIoDevice::savedata0 || device == VitaIoDevice::savedata1 || device == VitaIoDevice::app0
        || device == VitaIoDevice::_INVALID || device == VitaIoDevice::addcont0 || device == VitaIoDevice::cache0 || device == VitaIoDevice::tty0
        || device == VitaIoDevice::tty1 || device == VitaIoDevice::tty2 || device == VitaIoDevice::tty3
        || device == VitaIoDevice::music0 || device == VitaIoDevice::photo0 || device == VitaIoDevice::video0
        || device == VitaIoDevice::memory || device == VitaIoDevice::attach0 || device == VitaIoDevice::checkout0
        || device == VitaIoDevice::friend0 || device == VitaIoDevice::near0 || device == VitaIoDevice::psnmsg0
        || device == VitaIoDevice::sqlite0 || device == VitaIoDevice::webbrowser0);
}

bool init(IOState &io, const fs::path &cache_path, const fs::path &log_path, const fs::path &vita_fs_path, bool redirect_stdio) {
    {
        const std::lock_guard lock(io.async_mutex);
        io.async_stopping = false;
    }
    // Iterate through the entire list of devices and create the subdirectories if they do not exist
    boost::mp11::mp_for_each<boost::describe::describe_enumerators<VitaIoDevice>>([&vita_fs_path](auto i) {
        if (is_valid_output_path(i.value))
            fs::create_directories(vita_fs_path / i.name);
    });

    const fs::path ux0{ vita_fs_path / "ux0" };
    const fs::path uma0{ vita_fs_path / "uma0" };
    const fs::path vd0{ vita_fs_path / "vd0" };

    fs::create_directories(ux0 / "data");
    // The memory: work directory is volatile across emulator sessions.
    fs::remove_all(ux0 / "data" / "memory");
    fs::create_directories(ux0 / "data" / "memory");
    fs::create_directories(ux0 / "app");
    fs::create_directories(ux0 / "music");
    fs::create_directories(ux0 / "picture");
    fs::create_directories(ux0 / "theme");
    fs::create_directories(ux0 / "video");
    fs::create_directories(ux0 / "user");
    fs::create_directories(uma0 / "data");
    fs::create_directories(vd0 / "registry");
    fs::create_directories(vd0 / "network");

    fs::create_directories(cache_path / "shaders");
    fs::create_directory(log_path / "shaderlog");
    fs::create_directory(log_path / "texturelog");

    io.redirect_stdio = redirect_stdio;

#ifndef _WIN32
    io.case_isens_find_enabled = true;
#endif

    return true;
}

void io_deinit(IOState &io) {
    decltype(io.async_requests) pending;
    {
        const std::lock_guard lock(io.async_mutex);
        io.async_stopping = true;
        pending.swap(io.async_requests);
    }
    pending.clear();
    const std::lock_guard savedata_lock(io.savedata_mutex);

    {
        const std::lock_guard files_lock(io.fd_mutex);
        std::erase_if(io.std_files, [](const auto &entry) { return !entry.second->is_shell_file(); });
        std::erase_if(io.dir_entries, [](const auto &entry) { return !entry.second->is_shell_directory(); });
        io.tty_files.clear();
        if (io.std_files.empty() && io.dir_entries.empty())
            io.next_fd = 0;
    }

    io.device_paths = {};
    io.addcont.clear();
    io.content_id.clear();
    io.savedata.clear();
    io.title_id.clear();
    io.app_path.clear();

    io.cachemap.clear();

    {
        std::lock_guard lock(io.appmeta_mutex);
        std::erase_if(io.virtual_pfs_mounts, [](const auto &entry) { return entry.first.type != pfsmgr::MountType::ThemeData; });
        io.addcont_pfs_mount.clear();
        io.app_game_base_mount.clear();
        io.app_game_patch_mount.clear();
        io.savedata_pfs_mount.clear();
        io.savedata_access.reset();
    }

    {
        std::lock_guard<std::mutex> lock(io.overlay_mutex);
        io.overlays.clear();
        io.next_overlay_id = 1;
        io.app_overlay_id = -1;
    }
}

void init_device_paths(IOState &io, const fs::path &vita_fs_path) {
    io.device_paths.user_id = io.user_id;
    io.device_paths.savedata0 = "user/" + io.user_id + "/savedata/" + io.savedata;
    const auto app_device = device::get_device(io.app_path);
    if (!io.app_path.empty() && app_device != VitaIoDevice::_INVALID && app_device != VitaIoDevice::app0) {
        io.device_paths.app0_device = app_device;
        io.device_paths.app0 = device::remove_device_from_path(io.app_path, app_device);
    } else if (!io.app_path.empty() && app_device == VitaIoDevice::_INVALID && io.app_path.find(':') == std::string::npos) {
        io.device_paths.app0_device = VitaIoDevice::ux0;
        io.device_paths.app0 = "app/" + io.app_path;
    } else {
        LOG_ERROR("Cannot mount app0 for invalid application path '{}'.", io.app_path);
        io.device_paths.app0_device = VitaIoDevice::_INVALID;
        io.device_paths.app0.clear();
    }
    io.device_paths.addcont0 = "addcont/" + io.addcont;
    io.device_paths.cache0.clear();
    {
        std::lock_guard<std::mutex> lock(io.overlay_mutex);
        std::erase_if(io.overlays, [&](const FiosOverlay &overlay) { return overlay.id == io.app_overlay_id; });
        io.app_overlay_id = -1;
    }
    if (app_device != VitaIoDevice::vs0 && !io.title_id.empty() && fs::is_directory(vita_fs_path / "ux0" / "patch" / io.title_id)) {
        SceFiosProcessOverlay overlay{};
        overlay.type = SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT;
        overlay.order = 0x82;
        const auto src = "ux0:/patch/" + io.title_id + "/";
        if (src.size() >= sizeof(overlay.src)) {
            LOG_ERROR("Cannot mount update for '{}': path is too long", io.title_id);
            return;
        }
        std::strcpy(overlay.dst, "app0:/");
        std::memcpy(overlay.src, src.c_str(), src.size() + 1);
        io.app_overlay_id = create_overlay(io, &overlay);
    }
}

bool init_savedata_app_path(IOState &io, const fs::path &vita_fs_path, const std::function<pfsmgr::SavedataIdentity()> &identity_provider) {
    const std::lock_guard savedata_lock(io.savedata_mutex);
    const fs::path user_id_path{ vita_fs_path / "ux0" / "user" / io.user_id };
    const fs::path savedata_path{ user_id_path / "savedata" };
    const fs::path savedata_game_path{ savedata_path / io.savedata };

    try {
        const auto status = fs::symlink_status(savedata_game_path);
        if (!fs::exists(status)) {
            fs::create_directories(savedata_path);
            const auto identity = identity_provider();
            pfsmgr::create_savedata(savedata_game_path, { io.savedata, io.title_id, identity });
            LOG_INFO("Created protected savedata for '{}' using the configured savedata identity", io.title_id);
        }
    } catch (const std::runtime_error &error) {
        LOG_ERROR("Cannot create protected savedata '{}': {}", savedata_game_path, error.what());
        return false;
    } catch (const std::bad_alloc &) {
        LOG_ERROR("Cannot create protected savedata '{}': out of memory", savedata_game_path);
        return false;
    }

    for (const auto *name : { "files.db", "unicv.db", "icv.db" }) {
        const auto metadata = savedata_game_path / "sce_pfs" / name;
        std::error_code error;
        const auto status = fs::symlink_status(metadata, error);
        if (error && error != std::errc::no_such_file_or_directory) {
            LOG_ERROR("Cannot inspect savedata PFS metadata '{}': {}", metadata, error.message());
            return false;
        }
        if (!error && fs::exists(status)) {
            try {
                const auto name = pfsmgr::allocate_mount_name("gp");
                if (name.empty())
                    return false;
                auto access = std::make_shared<SavedataAccess>();
                const auto journal = savedata_game_path / "sce_pfs" / ".vita3k-transaction";
                if (fs::exists(fs::symlink_status(journal)))
                    access->store_session(std::make_shared<pfsmgr::PfsSession>(savedata_game_path));
                if (!io.pfs_mounts.mount_savedata_directory(name, savedata_game_path))
                    return false;
                std::lock_guard lock(io.appmeta_mutex);
                io.savedata_pfs_mount = name;
                io.savedata_access = std::move(access);
                LOG_WARN("Mounted protected savedata '{}' (AppUtil writes are experimental)", savedata_game_path);
                return true;
            } catch (const std::runtime_error &mount_error) {
                LOG_ERROR("Cannot mount protected savedata '{}': {}", savedata_game_path, mount_error.what());
                return false;
            }
        }
    }

    return true;
}

template <typename Mutation>
static int mutate_protected_savedata(IOState &io, Mutation &&mutation) {
    const std::lock_guard savedata_lock(io.savedata_mutex);
    std::shared_ptr<SavedataAccess> access;
    std::optional<pfsmgr::MountedDirectory> mount;
    std::string name;
    {
        const std::lock_guard lock(io.appmeta_mutex);
        name = io.savedata_pfs_mount;
        access = io.savedata_access;
        if (!name.empty())
            mount = io.pfs_mounts.get_directory(name);
    }
    if (!access || !mount) {
        LOG_ERROR("Protected savedata mount is unavailable");
        return SCE_ERROR_ERRNO_EBADFD;
    }
    std::shared_ptr<pfsmgr::PfsSession> session;
    bool mutation_completed = false;
    try {
        if (access->needs_recovery.load())
            throw std::runtime_error("savedata requires recovery; restart the application");
        std::error_code ec;
        if (fs::canonical(mount->source, ec) != mount->canonical_source || ec)
            throw std::runtime_error("savedata mount path changed");
        session = access->load_session();
        if (!session) {
            session = std::make_shared<pfsmgr::PfsSession>(mount->source);
            access->store_session(session);
        }
        access->needs_recovery.store(true);
        mutation(*session);
        mutation_completed = true;
        if (!io.pfs_mounts.refresh_savedata_directory(name))
            return SCE_ERROR_ERRNO_EIO;
        access->needs_recovery.store(false);
        return 0;
    } catch (const std::runtime_error &error) {
        if (!mutation_completed && session && session->is_usable()) {
            if (io.pfs_mounts.refresh_savedata_directory(name))
                access->needs_recovery.store(false);
        }
        LOG_ERROR("Protected savedata mutation failed: {}", error.what());
        return SCE_ERROR_ERRNO_EIO;
    } catch (const std::bad_alloc &) {
        LOG_ERROR("Protected savedata mutation ran out of memory");
        return SCE_ERROR_ERRNO_ENOMEM;
    }
}

int create_protected_savedata_directories(IOState &io, const fs::path &relative_path) {
    return mutate_protected_savedata(io, [&](pfsmgr::PfsSession &session) { session.create_directories(relative_path); });
}

int write_protected_savedata(IOState &io, const fs::path &relative_path, uint64_t offset,
    std::span<const uint8_t> data, std::optional<uint64_t> truncate_size) {
    return mutate_protected_savedata(io, [&](pfsmgr::PfsSession &session) {
        session.write(relative_path, offset, data, true, false, truncate_size);
    });
}

int remove_protected_savedata(IOState &io, const fs::path &relative_path) {
    return mutate_protected_savedata(io, [&](pfsmgr::PfsSession &session) { session.remove(relative_path); });
}

bool find_case_isens_path(IOState &io, VitaIoDevice &device, const fs::path &translated_path, const fs::path &system_path) {
    std::string final_path{};

    switch (device) {
    case VitaIoDevice::app0: {
        std::string app_id = translated_path.string().substr(0, 14);
        final_path = system_path.string().substr(0, system_path.string().find(app_id)) + app_id;
        break;
    }
    case VitaIoDevice::addcont0: {
        std::string addcont_id = translated_path.string().substr(0, 18);
        final_path = system_path.string().substr(0, system_path.string().find(addcont_id)) + addcont_id;
        break;
    }
    case VitaIoDevice::vs0: {
        // This only works if ALL the parent folders of the path are the correct case or are in a case insensitive fs
        // Only the file's name is searched for, not the parent folders
        final_path = system_path.string().substr(0, system_path.string().find_last_of('/'));
        break;
    }
    default: {
        return false;
    }
    }

    if (!fs::exists(final_path))
        return false;

    for (const auto &file : fs::recursive_directory_iterator(final_path)) {
        io.cachemap.emplace(string_utils::tolower(file.path().string()), file.path().string());
    }

    return true;
}

fs::path find_in_cache(IOState &io, const std::string &system_path) {
    const auto find_path = io.cachemap.find(system_path);

    if (find_path != io.cachemap.end()) {
        return fs::path{ find_path->second.c_str() };
    } else {
        return fs::path{};
    }
}

std::string translate_path(const char *path, VitaIoDevice &device, const IOState::DevicePaths &device_paths) {
    auto relative_path = device::remove_duplicate_device(path, device);

    // replace invalid slashes with proper forward slash
    string_utils::replace(relative_path, "\\", "/");
    string_utils::replace(relative_path, "/./", "/");
    string_utils::replace(relative_path, "//", "/");
    // TODO: Handle dot-dot paths

    switch (device) {
    case VitaIoDevice::savedata0: // Redirect savedata0: to ux0:user/00/savedata/<title_id>
    case VitaIoDevice::savedata1: {
        relative_path = device::remove_device_from_path(relative_path, device, device_paths.savedata0);
        device = VitaIoDevice::ux0;
        break;
    }
    case VitaIoDevice::app0: {
        if (device_paths.app0_device == VitaIoDevice::_INVALID || device_paths.app0.empty())
            return {};
        relative_path = device::remove_device_from_path(relative_path, device, device_paths.app0);
        device = device_paths.app0_device;
        break;
    }
    case VitaIoDevice::addcont0: { // Redirect addcont0: to ux0:addcont/<title_id>
        relative_path = device::remove_device_from_path(relative_path, device, device_paths.addcont0);
        device = VitaIoDevice::ux0;
        break;
    }
    case VitaIoDevice::cache0: { // Redirect cache0: to ux0:cache/<title_id>
        if (device_paths.cache0.empty())
            return std::string{};
        relative_path = device::remove_device_from_path(relative_path, device, device_paths.cache0);
        device = VitaIoDevice::ux0;
        break;
    }
    case VitaIoDevice::friend0:
    case VitaIoDevice::psnmsg0:
    case VitaIoDevice::near0: {
        const char *directory = device == VitaIoDevice::friend0 ? "psnfriend" : device == VitaIoDevice::psnmsg0 ? "psnmsg"
                                                                                                                : "near";
        relative_path = device::remove_device_from_path(relative_path, device, "user/" + device_paths.user_id + "/" + directory);
        device = VitaIoDevice::ur0;
        break;
    }
    case VitaIoDevice::sqlite0:
    case VitaIoDevice::attach0:
    case VitaIoDevice::checkout0:
    case VitaIoDevice::webbrowser0: {
        const char *directory = device == VitaIoDevice::sqlite0 ? "sqlite" : device == VitaIoDevice::attach0 ? "attach"
            : device == VitaIoDevice::checkout0                                                              ? "checkout"
                                                                                                             : "webbrowser";
        relative_path = device::remove_device_from_path(relative_path, device, std::string("temp/") + directory);
        device = VitaIoDevice::ur0;
        break;
    }
    case VitaIoDevice::music0: { // Redirect music0: to ux0:music
        relative_path = device::remove_device_from_path(relative_path, device, "music");
        device = VitaIoDevice::ux0;
        break;
    }
    case VitaIoDevice::photo0: { // Redirect photo0: to ux0:picture
        relative_path = device::remove_device_from_path(relative_path, device, "picture");
        device = VitaIoDevice::ux0;
        break;
    }
    case VitaIoDevice::memory: { // Redirect memory: to ux0:data/memory
        relative_path = device::remove_device_from_path(relative_path, device, "data/memory");
        device = VitaIoDevice::ux0;
        break;
    }
    case VitaIoDevice::video0: { // Redirect video0: to ux0:video
        relative_path = device::remove_device_from_path(relative_path, device, "video");
        device = VitaIoDevice::ux0;
        break;
    }

    case VitaIoDevice::host0:
    case VitaIoDevice::gro0:
    case VitaIoDevice::grw0:
    case VitaIoDevice::imc0:
    case VitaIoDevice::os0:
    case VitaIoDevice::pd0:
    case VitaIoDevice::sa0:
    case VitaIoDevice::sd0:
    case VitaIoDevice::tm0:
    case VitaIoDevice::ud0:
    case VitaIoDevice::uma0:
    case VitaIoDevice::ur0:
    case VitaIoDevice::ux0:
    case VitaIoDevice::vd0:
    case VitaIoDevice::vs0:
    case VitaIoDevice::xmc0: {
        relative_path = device::remove_device_from_path(relative_path, device);
        break;
    }
    case VitaIoDevice::tty0:
    case VitaIoDevice::tty1:
    case VitaIoDevice::tty2:
    case VitaIoDevice::tty3: {
        return std::string{};
    }
    default: {
        LOG_CRITICAL_IF(relative_path.contains(':'), "Unknown device with path {} used. Report this to the developers!", relative_path);
        return std::string{};
    }
    }

    // If the path is empty, the request is the device itself
    if (relative_path.empty())
        return std::string{};

    if (relative_path.front() == '/' || relative_path.front() == '\\')
        relative_path.erase(0, 1);

    return relative_path;
}

static fs::path expand_unresolved_path(IOState &io, const char *path, const fs::path &vita_fs_path) {
    if (is_appmeta_path(path))
        return appmeta_host_path(io, path);
    auto device = device::get_device(path);
    if (device == VitaIoDevice::_INVALID)
        return {};

    const auto translated_path = translate_path(path, device, io.device_paths);
    return device::construct_emulated_path(device, translated_path, vita_fs_path, io.redirect_stdio);
}

static fs::path find_host_path(IOState &io, const fs::path &path) {
    if (!io.case_isens_find_enabled || fs::exists(path))
        return path;

    auto current = path.root_path();
    for (const auto &part : path.relative_path()) {
        const auto exact = current / part;
        if (fs::exists(exact)) {
            current = exact;
            continue;
        }
        if (!fs::is_directory(current))
            return path;
        const auto wanted = string_utils::tolower(fs_utils::path_to_utf8(part));
        bool found = false;
        for (const auto &entry : fs::directory_iterator(current)) {
            if (string_utils::tolower(fs_utils::path_to_utf8(entry.path().filename())) == wanted) {
                current = entry.path();
                found = true;
                break;
            }
        }
        if (!found)
            return path;
    }
    return current;
}

fs::path expand_path(IOState &io, const char *path, const fs::path &vita_fs_path) {
    {
        std::lock_guard lock(io.appmeta_mutex);
        if (!io.savedata_pfs_mount.empty() && std::string_view(path).starts_with("savedata0:")) {
            LOG_WARN("Cannot expose protected savedata as a host path: '{}'", path);
            return {};
        }
    }
    if (is_appmeta_path(path)) {
        const auto host = appmeta_host_path(io, path);
        if (!host.empty()) {
            const auto resolved = game_data_layer(io, path, host);
            if (!resolved || (resolved->layer && resolved->layer->mode() == pfsmgr::Mode::Mode5 && resolved->scope == pfsmgr::MountScope::Shell)) {
                LOG_WARN("Cannot expose protected appmeta as a host path: '{}'", path);
                return {};
            }
        }
        return host.empty() || overlaps_savedata_mount(io, host) ? fs::path{} : host;
    }
    const auto resolved = resolve_path(io, path, vita_fs_path);
    if (std::string_view(path).starts_with("addcont0:") && is_appmeta_path(resolved)) {
        const auto host = appmeta_host_path(io, resolved.c_str());
        if (host.empty())
            return {};
        const auto layer = game_data_layer(io, resolved.c_str(), host);
        if (!layer || layer->layer) {
            LOG_WARN("Cannot expose protected additional content as a host path: '{}'", path);
            return {};
        }
        return host;
    }
    const auto host = find_host_path(io, expand_unresolved_path(io, resolved.c_str(), vita_fs_path));
    return host.empty() || overlaps_savedata_mount(io, host) ? fs::path{} : host;
}

static std::shared_ptr<FileStats> find_file(const IOState &io, const SceUID fd) {
    std::lock_guard lock(io.fd_mutex);
    const auto file = io.std_files.find(fd);
    return file == io.std_files.end() ? nullptr : file->second;
}

static int host_open_error(const int error) {
    switch (error) {
    case ENOENT: return SCE_ERROR_ERRNO_ENOENT;
    case EACCES: return SCE_ERROR_ERRNO_EACCES;
    case EMFILE: return SCE_ERROR_ERRNO_EMFILE;
    case EEXIST: return SCE_ERROR_ERRNO_EEXIST;
    case ENOMEM: return SCE_ERROR_ERRNO_ENOMEM;
    case ENOSPC: return SCE_ERROR_ERRNO_ENOSPC;
    default: return SCE_ERROR_ERRNO_EIO;
    }
}

SceUID open_file(IOState &io, const char *path, const int flags, const fs::path &vita_fs_path, const char *export_name, bool quiet_missing_mount) {
    const std::lock_guard savedata_lock(io.savedata_mutex);
    if (is_appmeta_path(path)) {
        if (!io.savedata_pfs_mount.empty() && std::string_view(path).starts_with(io.savedata_pfs_mount)
            && io.savedata_access && io.savedata_access->needs_recovery.load())
            return IO_ERROR(SCE_ERROR_ERRNO_EIO);
        if (can_write(flags) || (flags & SCE_O_TRUNC))
            return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
        const auto host = appmeta_host_path(io, path);
        std::error_code ec;
        const bool regular_file = !host.empty() && fs::is_regular_file(host, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) {
            LOG_ERROR("Cannot inspect mounted file '{}': {}", path, ec.message());
            return IO_ERROR(SCE_ERROR_ERRNO_EIO);
        }
        if (!regular_file)
            return quiet_missing_mount ? SCE_ERROR_ERRNO_ENOENT : IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
        const auto resolved_layer = game_data_layer(io, path, host);
        if (!resolved_layer)
            return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
        const auto &[layer, relative, lease, scope] = *resolved_layer;
        if (layer) {
            try {
                auto pfs_file = layer->open_file(relative);
                if (!pfs_file)
                    return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                auto file = std::make_shared<FileStats>(path, host, flags, std::move(pfs_file));
                if (!io.savedata_pfs_mount.empty() && std::string_view(path).starts_with(io.savedata_pfs_mount) && io.savedata_access)
                    file->set_savedata_access(io.savedata_access, relative);
                file->hold_mount(lease);
                file->set_mount_scope(scope);
                const std::lock_guard lock(io.fd_mutex);
                const auto fd = io.next_fd++;
                io.std_files.emplace(fd, std::move(file));
                return fd;
            } catch (const std::runtime_error &error) {
                LOG_ERROR("Cannot open PFS file '{}': {}", path, error.what());
                return IO_ERROR(SCE_ERROR_ERRNO_EIO);
            }
        }
        auto stream = create_file(host, flags);
        if (!stream)
            return IO_ERROR(host_open_error(errno));
        auto file = std::make_shared<FileStats>(path, path, host, flags, std::move(stream));
        file->hold_mount(lease);
        file->set_mount_scope(scope);
        SceUID fd;
        {
            const std::lock_guard lock(io.fd_mutex);
            fd = io.next_fd++;
            io.std_files.emplace(fd, std::move(file));
        }
        return fd;
    }
    auto device = device::get_device(path);
    auto device_for_icase = device;
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    if ((device == VitaIoDevice::tty0) || (device == VitaIoDevice::tty1) || (device == VitaIoDevice::tty2) || (device == VitaIoDevice::tty3)) {
        assert(flags >= 0);

        auto tty_type = TTY_UNKNOWN;
        if (flags & SCE_O_RDONLY)
            tty_type |= TTY_IN;
        if (flags & SCE_O_WRONLY)
            tty_type |= TTY_OUT;

        SceUID fd;
        {
            std::lock_guard lock(io.fd_mutex);
            fd = io.next_fd++;
            io.tty_files.emplace(fd, tty_type);
        }

        LOG_TRACE_IF(log_file_op, "{}: Opening terminal {}:", export_name, device);
        return fd;
    }

    const auto resolved_path = resolve_path(io, path, vita_fs_path, can_write(flags));
    if (is_appmeta_path(resolved_path))
        return open_file(io, resolved_path.c_str(), flags, vita_fs_path, export_name, quiet_missing_mount);
    device = device::get_device(resolved_path);
    const auto translated_path = translate_path(resolved_path.c_str(), device, io.device_paths);
    if (translated_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    auto system_path = find_host_path(io, device::construct_emulated_path(device, translated_path, vita_fs_path, io.redirect_stdio));
    if (overlaps_savedata_mount(io, system_path)) {
        LOG_ERROR("Refusing direct access to protected savedata path '{}'", path);
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    }
    if (fs::is_directory(system_path)) {
        LOG_ERROR("Cannot open directory: {}", system_path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    // Do not allow any new files if they do not have a write flag.
    if (!fs::exists(system_path)) {
        if (!(flags & SCE_O_CREAT)) {
            if (io.case_isens_find_enabled) {
                // Attempt a case-insensitive file search.
                const auto original_system_path = system_path;
                const auto cached_path = find_in_cache(io, string_utils::tolower(system_path.string()));
                if (!cached_path.empty()) {
                    system_path = cached_path;
                    LOG_TRACE("Found cached filepath at {}", system_path);
                } else {
                    const bool path_found = find_case_isens_path(io, device_for_icase, translated_path, system_path);
                    system_path = find_in_cache(io, string_utils::tolower(system_path.string()));
                    if (!system_path.empty() && path_found) {
                        LOG_TRACE("Found file on case-sensitive filesystem at {}", system_path);
                    } else {
                        LOG_ERROR("Missing file at {} (target path: {})", original_system_path, path);
                        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                    }
                }
            } else {
                LOG_ERROR("Missing file at {} (target path: {})", system_path, path);
                return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
            }
        } else {
            if (!fs::exists(system_path.parent_path())) {
                fs::create_directories(system_path.parent_path());
            }
            std::ofstream file(system_path);
        }
    }

    const auto normalized_path = device::construct_normalized_path(device, translated_path);

    auto stream = create_file(system_path, flags);
    if (!stream) {
        const int error = errno;
        LOG_ERROR("Failed to open file {} (target path: {}), host errno {}", system_path, path, error);
        return IO_ERROR(host_open_error(error));
    }

    auto file = std::make_shared<FileStats>(path, normalized_path, system_path, flags, std::move(stream));
    SceUID fd;
    {
        std::lock_guard lock(io.fd_mutex);
        fd = io.next_fd++;
        io.std_files.emplace(fd, std::move(file));
    }

    LOG_TRACE_IF(log_file_op, "{}: Opening file {} ({}), fd: {}", export_name, path, normalized_path, log_hex(fd));
    return fd;
}

int read_file(void *data, IOState &io, const SceUID fd, const SceSize size, const char *export_name) {
    const std::lock_guard savedata_lock(io.savedata_mutex);
    assert(data != nullptr);
    assert(size >= 0);

    const auto file = find_file(io, fd);
    if (file) {
        std::lock_guard lock(file->mutex);
        if (file->closed)
            return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
        const auto read = file->read(data, 1, size);
        LOG_TRACE_IF(log_file_op && log_file_read, "{}: Reading {} bytes of fd {}", export_name, read, log_hex(fd));
        if (read < 0)
            return IO_ERROR(SCE_ERROR_ERRNO_EIO);
        return static_cast<int>(read);
    }

    TtyType tty_type = TTY_UNKNOWN;
    bool tty_found = false;
    {
        std::lock_guard lock(io.fd_mutex);
        const auto tty_file = io.tty_files.find(fd);
        if (tty_file != io.tty_files.end()) {
            tty_found = true;
            tty_type = tty_file->second;
        }
    }
    if (tty_found) {
        if (tty_type == TTY_IN) {
            std::cin.read(static_cast<char *>(data), size);
            LOG_TRACE_IF(log_file_op && log_file_read, "{}: Reading terminal fd: {}, size: {}", export_name, log_hex(fd), size);
            return size;
        }
        return IO_ERROR_UNK();
    }

    return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
}

bool read_file_to_buffer(IOState &io, const char *path, const fs::path &vita_fs_path, std::vector<uint8_t> &buffer, const char *export_name, size_t max_size, int *open_error, bool quiet_missing_mount) {
    buffer.clear();
    const auto fd = open_file(io, path, SCE_O_RDONLY, vita_fs_path, export_name, quiet_missing_mount);
    if (open_error)
        *open_error = fd < 0 ? fd : 0;
    if (fd < 0)
        return false;
    struct FileCloser {
        IOState &io;
        SceUID fd;
        const char *name;
        ~FileCloser() {
            if (close_file(io, fd, name) < 0)
                LOG_WARN("Failed to close file descriptor {}", fd);
        }
    } closer{ io, fd, export_name };
    const auto size = seek_file(fd, 0, SCE_SEEK_END, io, export_name);
    if (size < 0 || static_cast<uint64_t>(size) > buffer.max_size() || static_cast<uint64_t>(size) > max_size || seek_file(fd, 0, SCE_SEEK_SET, io, export_name) < 0) {
        LOG_ERROR("Cannot determine file size for fd {}", fd);
        return false;
    }
    buffer.resize(static_cast<size_t>(size));
    size_t offset = 0;
    while (offset < buffer.size()) {
        const auto count = static_cast<SceSize>(std::min<size_t>(buffer.size() - offset, 1024 * 1024));
        const auto read = read_file(buffer.data() + offset, io, fd, count, export_name);
        if (read <= 0) {
            LOG_ERROR("Cannot read complete file for fd {}", fd);
            buffer.clear();
            return false;
        }
        offset += static_cast<size_t>(read);
    }
    return true;
}

constexpr int ASYNC_BUSY = static_cast<int>(0x80010010u);
constexpr int ASYNC_READ_ERROR = static_cast<int>(0x80010005u);
constexpr uint32_t ASYNC_RESULT_SIZE = 24;
constexpr uint32_t GUEST_PAGE_SIZE = 4096;

struct GuestPin {
    MemState *mem;
    uint64_t id;

    GuestPin(MemState &state, uint64_t pin)
        : mem(&state)
        , id(pin) {}
    GuestPin(const GuestPin &) = delete;
    GuestPin &operator=(const GuestPin &) = delete;
    GuestPin(GuestPin &&other) noexcept
        : mem(other.mem)
        , id(other.id) { other.id = 0; }
    ~GuestPin() {
        if (id)
            unpin_guest_memory(*mem, id);
    }
};

struct AsyncFileLease {
    std::shared_ptr<FileStats> file;

    explicit AsyncFileLease(std::shared_ptr<FileStats> opened)
        : file(std::move(opened)) {
        if (file) {
            bool closed;
            {
                std::lock_guard lock(file->mutex);
                closed = file->closed;
                if (!closed)
                    ++file->pending_async_reads;
            }
            if (closed)
                file.reset();
        }
    }
    AsyncFileLease(const AsyncFileLease &) = delete;
    AsyncFileLease &operator=(const AsyncFileLease &) = delete;
    AsyncFileLease(AsyncFileLease &&) = default;
    ~AsyncFileLease() {
        if (!file)
            return;
        std::lock_guard lock(file->mutex);
        if (--file->pending_async_reads == 0 && file->closed && file->close() != 0)
            LOG_ERROR("Failed to close file after asynchronous read: host errno {}", errno);
    }
};

static void copy_guest(MemState &mem, Address address, void *buffer, size_t size, bool to_guest) {
    auto *bytes = static_cast<uint8_t *>(buffer);
    while (size) {
        const size_t count = std::min(size, size_t(GUEST_PAGE_SIZE - (address % GUEST_PAGE_SIZE)));
        auto *guest = Ptr<uint8_t>(address).get(mem);
        if (to_guest)
            std::memcpy(guest, bytes, count);
        else
            std::memcpy(bytes, guest, count);
        address += static_cast<Address>(count);
        bytes += count;
        size -= count;
    }
}

int complete_async_io(IOState &io, SceUID request_id, const char *export_name) {
    std::shared_ptr<AsyncIoRequest> request;
    {
        const std::lock_guard lock(io.async_mutex);
        const auto it = io.async_requests.find(request_id);
        if (it == io.async_requests.end())
            return IO_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
        SceUInt32 pattern = 0;
        const int error = it->second->event->get_pattern(pattern);
        if (error)
            return IO_ERROR(error);
        // A pending operation cannot be destroyed safely by the emulator.
        if (!it->second->finished || pattern != 0)
            return IO_ERROR(ASYNC_BUSY);
        request = std::move(it->second);
        io.async_requests.erase(it);
    }
    request.reset();
    return 0;
}

static std::string standardize_path(std::string_view path) {
    // replace app0:... by app0:/...
    bool start_with_app0 = path.starts_with("app0:");
    if (start_with_app0 && (path.size() == 5 || path[5] != '/'))
        return "app0:/" + std::string(path.substr(5));
    else
        return std::string(path);
}

SceUID create_overlay(IOState &io, SceFiosProcessOverlay *fios_overlay) {
    std::lock_guard<std::mutex> lock(io.overlay_mutex);

    FiosOverlay overlay{
        .id = io.next_overlay_id++,
        .type = fios_overlay->type,
        .order = fios_overlay->order,
        .process_id = fios_overlay->process_id,
        .dst = standardize_path(fios_overlay->dst),
        .src = standardize_path(fios_overlay->src)
    };

    // lower order first and in case of equality, last one inserted first
    size_t overlay_index = 0;
    while (overlay_index < io.overlays.size() && io.overlays[overlay_index].order < overlay.order)
        overlay_index++;
    auto res = overlay.id;
    io.overlays.insert(io.overlays.begin() + overlay_index, std::move(overlay));
    return res;
}

std::vector<std::string> resolve_overlay_paths(IOState &io, const char *input, const fs::path &vita_fs_path, const bool for_write, const SceUInt32 min_order, const SceUInt32 max_order, const bool for_directory) {
    const auto resolve_mount = [&](std::string path) {
        const auto colon = path.find(':');
        if (colon == std::string::npos)
            return path;
        std::lock_guard lock(io.appmeta_mutex);
        return resolve_mount_alias(io, path.substr(0, colon + 1)) + path.substr(colon + 1);
    };
    if (is_appmeta_path(input))
        return { resolve_mount(input) };
    {
        std::lock_guard lock(io.appmeta_mutex);
        const std::string_view path(input);
        if (!io.savedata_pfs_mount.empty() && path.starts_with("savedata0:"))
            return { io.savedata_pfs_mount + std::string(path.substr(10)) };
        if (!io.savedata_pfs_mount.empty() && path.starts_with("savedata1:"))
            return { "PD000000000000d0:" };
    }
    const std::string addcont_input(input);
    if (addcont_input.starts_with("addcont0:")) {
        auto relative = addcont_input.substr(9);
        while (!relative.empty() && relative.front() == '/')
            relative.erase(0, 1);
        if (relative.find_first_of("\\:") != std::string::npos)
            return { "PD000000000000d0:" };
        for (const auto &component : fs::path(relative)) {
            if (component == "." || component == "..")
                return { "PD000000000000d0:" };
        }
        const auto slash = relative.find('/');
        const auto entitlement = relative.substr(0, slash);
        if (!entitlement.empty()) {
            {
                std::lock_guard lock(io.appmeta_mutex);
                const auto mount = io.virtual_pfs_mounts.find({ pfsmgr::MountType::AddCont, entitlement });
                if (mount != io.virtual_pfs_mounts.end())
                    return { mount->second + (slash == std::string::npos ? "" : relative.substr(slash)) };
            }
            if (slash != std::string::npos) {
                bool protected_addcont = false;
                if (!pfsmgr::has_databases(vita_fs_path / "ux0" / "addcont" / io.addcont / entitlement, protected_addcont) || protected_addcont)
                    return { "PD000000000000d0:" };
            }
        }
    }
    std::lock_guard<std::mutex> lock(io.overlay_mutex);

    const auto modification_time = [&](const std::string &path) -> std::optional<std::chrono::seconds> {
        const auto host_path = find_host_path(io, expand_unresolved_path(io, path.c_str(), vita_fs_path));
        if (host_path.empty())
            return std::nullopt;
        std::error_code ec;
        const auto time = fs::last_write_time(host_path, ec);
        if (ec) {
            if (ec != std::errc::no_such_file_or_directory && ec != std::errc::not_a_directory)
                LOG_WARN("Cannot stat overlay path '{}': {}", host_path, ec.message());
            return std::nullopt;
        }
        return std::chrono::floor<std::chrono::seconds>(time.time_since_epoch());
    };

    const bool application_path = standardize_path(input).starts_with("app0:");
    const auto resolve_from = [&](auto &&self, std::string curr_path, size_t overlay_idx) -> std::vector<std::string> {
        while (overlay_idx < io.overlays.size()) {
            if (is_appmeta_path(curr_path))
                break;
            const FiosOverlay &overlay = io.overlays[overlay_idx++];
            if (overlay.order < min_order)
                continue;
            if (overlay.order > max_order)
                break;
            if (!curr_path.starts_with(overlay.dst)
                || (curr_path.size() > overlay.dst.size() && !overlay.dst.ends_with('/') && curr_path[overlay.dst.size()] != '/'))
                continue;

            const auto candidate = overlay.src + curr_path.substr(overlay.dst.size());
            if (overlay.type == SCE_FIOS_OVERLAY_TYPE_OPAQUE || (overlay.type == SCE_FIOS_OVERLAY_TYPE_WRITABLE && for_write && !for_directory)) {
                curr_path = candidate;
            } else {
                auto candidates = self(self, candidate, overlay_idx);
                auto fallback = self(self, curr_path, overlay_idx);
                if (!for_write && overlay.type == SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT) {
                    for (const auto &path : candidates) {
                        const auto host_path = find_host_path(io, expand_unresolved_path(io, path.c_str(), vita_fs_path));
                        if (fs::exists(host_path))
                            continue;
                        auto parent = host_path.parent_path();
                        while (!parent.empty() && parent != parent.root_path()) {
                            if (fs::is_regular_file(find_host_path(io, parent)))
                                return std::vector<std::string>{ path };
                            parent = parent.parent_path();
                        }
                    }
                }
                std::erase_if(candidates, [&](const std::string &path) {
                    return !fs::exists(find_host_path(io, expand_unresolved_path(io, path.c_str(), vita_fs_path)))
                        && !game_data_path_is_tracked(io, path);
                });
                if (overlay.type == SCE_FIOS_OVERLAY_TYPE_NEWER && !for_directory && !candidates.empty()) {
                    const auto candidate_time = modification_time(candidates.front());
                    const auto fallback_time = modification_time(fallback.front());
                    if (!candidate_time || (fallback_time && *fallback_time >= *candidate_time))
                        return fallback;
                }
                candidates.insert(candidates.end(), fallback.begin(), fallback.end());
                return candidates;
            }
        }
        if (application_path && !io.app_game_base_mount.empty()) {
            if (curr_path.starts_with("app0:"))
                curr_path = io.app_game_base_mount + curr_path.substr(5);
            else if (!io.app_game_patch_mount.empty()) {
                const auto patch_prefix = "ux0:/patch/" + io.title_id + "/";
                if (curr_path.starts_with(patch_prefix))
                    curr_path = io.app_game_patch_mount + "/" + curr_path.substr(patch_prefix.size());
            }
        }
        return { resolve_mount(std::move(curr_path)) };
    };

    return resolve_from(resolve_from, standardize_path(input), 0);
}

std::string resolve_path(IOState &io, const char *input, const fs::path &vita_fs_path, const bool for_write, const SceUInt32 min_order, const SceUInt32 max_order) {
    return resolve_overlay_paths(io, input, vita_fs_path, for_write, min_order, max_order).front();
}

int read_file_async(IOState &io, KernelState &kernel, MemState &mem, SceUID thread_id, SceUID fd,
    Ptr<uint8_t> data, SceSize size, Ptr<uint8_t> result, const char *export_name) {
    const std::lock_guard submit_lock(io.async_mutex);
    if (io.async_stopping)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    if (size > std::numeric_limits<int32_t>::max())
        return IO_ERROR(SCE_KERNEL_ERROR_ILLEGAL_SIZE);
    AsyncFileLease lease(find_file(io, fd));
    if (!lease.file || !(lease.file->get_open_mode() & SCE_O_RDONLY))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    const std::array ranges{ GuestMemoryRange{ data.address(), size },
        GuestMemoryRange{ result.address(), result ? ASYNC_RESULT_SIZE : 0 } };
    uint64_t pin_id;
    try {
        pin_id = pin_guest_memory(mem, ranges);
    } catch (const std::bad_alloc &) {
        LOG_ERROR("sceIoReadAsync: could not pin guest memory");
        return IO_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    }
    if (!pin_id)
        return IO_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    GuestPin pin(mem, pin_id);
    uint32_t cookie = 0;
    if (result)
        copy_guest(mem, result.address() + 8, &cookie, sizeof(cookie), false);

    try {
        auto event = std::make_shared<SimpleEvent>(SCE_KERNEL_EVENT_ATTR_AUTO_RESET, "SceIoAsyncEvent", 0, thread_id, false);
        auto request = std::make_shared<AsyncIoRequest>();
        const SceUID id = kernel.get_next_uid();
        request->event = event;
        request->cleanup = [&kernel, id, thread_id] {
            kernel.objects.remove<SimpleEvent>(id, thread_id);
        };
        kernel.objects.add(event, id);
        io.async_requests.emplace(id, request);
        try {
            request->worker = std::thread([&io, &mem, request_ptr = request.get(), lease = std::move(lease),
                                              event, data, size, result, cookie, pin = std::move(pin)]() mutable {
                int64_t read_result;
                try {
                    std::vector<uint8_t> buffer(std::min<size_t>(size, 1024 * 1024));
                    size_t total = 0;
                    bool read_failed = false;
                    const std::lock_guard savedata_lock(io.savedata_mutex);
                    const std::lock_guard state_lock(lease.file->mutex);
                    const auto file_lock = lease.file->lock_operations();
                    while (!read_failed && total < size) {
                        const size_t requested = std::min(buffer.size(), size_t(size) - total);
                        const auto read = lease.file->read_unlocked(buffer.data(), requested);
                        if (read < 0) {
                            read_failed = true;
                            break;
                        }
                        const auto count = static_cast<size_t>(read);
                        if (count)
                            copy_guest(mem, data.address() + static_cast<Address>(total), buffer.data(), count, true);
                        total += count;
                        if (count < requested)
                            break;
                    }
                    if (!read_failed && (total == size || total != 0 || !lease.file->has_read_error_unlocked()))
                        read_result = static_cast<int64_t>(total);
                    else
                        read_result = ASYNC_READ_ERROR;
                } catch (const std::bad_alloc &) {
                    LOG_ERROR("sceIoReadAsync: cannot allocate {} bytes", size);
                    read_result = static_cast<int>(SCE_KERNEL_ERROR_NO_MEMORY);
                }
                if (result) {
                    uint32_t zero = 0;
                    copy_guest(mem, result.address(), &read_result, sizeof(read_result), true);
                    copy_guest(mem, result.address() + 8, &cookie, sizeof(cookie), true);
                    copy_guest(mem, result.address() + 20, &zero, sizeof(zero), true);
                }
                const std::lock_guard done_lock(io.async_mutex);
                request_ptr->finished = true;
                const SceUInt64 user_data = result ? result.address() : static_cast<SceUInt64>(read_result);
                const int error = event->set_system_event(1, user_data);
                if (error)
                    LOG_ERROR("sceIoReadAsync: failed to signal request {}: {}", event->uid, log_hex(error));
            });
        } catch (const std::system_error &) {
            io.async_requests.erase(id);
            throw;
        } catch (const std::bad_alloc &) {
            io.async_requests.erase(id);
            throw;
        }
        return id;
    } catch (const std::system_error &error) {
        LOG_ERROR("sceIoReadAsync: could not start worker: {}", error.what());
        return IO_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    } catch (const std::bad_alloc &) {
        LOG_ERROR("sceIoReadAsync: could not allocate request");
        return IO_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    }
}

int write_file(SceUID fd, const void *data, const SceSize size, const IOState &io, const char *export_name) {
    assert(data != nullptr);
    assert(size >= 0);

    if (fd < 0) {
        LOG_WARN("Error writing fd: {}, size: {}", log_hex(fd), size);
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    }

    TtyType tty_type = TTY_UNKNOWN;
    bool tty_found = false;
    {
        std::lock_guard lock(io.fd_mutex);
        const auto tty_file = io.tty_files.find(fd);
        if (tty_file != io.tty_files.end()) {
            tty_found = true;
            tty_type = tty_file->second;
        }
    }
    if (tty_found) {
        if (tty_type & TTY_OUT) {
            std::string s(static_cast<char const *>(data), size);

            // trim newline
            if (io.redirect_stdio) {
                std::cout << s;
            } else {
                if (s.back() == '\n')
                    s.pop_back();
                LOG_TRACE_IF(log_file_op, "*** TTY: {}", s);
            }

            return size;
        }
        return IO_ERROR_UNK();
    }

    const auto file = find_file(io, fd);
    if (!file)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    if (!fs::is_directory(file->get_system_location().parent_path())) {
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT); // TODO: Is it the right error code?
    }

    if (file->can_write_file()) {
        std::lock_guard lock(file->mutex);
        if (file->closed)
            return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
        const auto written = file->write(data, 1, size);
        LOG_TRACE_IF(log_file_op, "{}: Writing to fd: {}, size: {}", export_name, log_hex(fd), size);
        return static_cast<int>(written);
    }

    return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
}

int truncate_file(const SceUID fd, unsigned long long length, const IOState &io, const char *export_name) {
    if (fd < 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    const auto file = find_file(io, fd);
    if (!file)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    std::lock_guard lock(file->mutex);
    if (file->closed || !file->can_write_file())
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    auto trunc = file->truncate(length);
    LOG_TRACE_IF(log_file_op, "{}: Truncating fd: {}, to size: {}", export_name, log_hex(fd), length);
    return trunc;
}

SceOff seek_file(const SceUID fd, const SceOff offset, const SceIoSeekMode whence, IOState &io, const char *export_name) {
    const std::lock_guard savedata_lock(io.savedata_mutex);
    if (!(whence == SCE_SEEK_SET || whence == SCE_SEEK_CUR || whence == SCE_SEEK_END))
        return IO_ERROR(SCE_ERROR_ERRNO_EOPNOTSUPP);

    if (fd < 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    const auto file = find_file(io, fd);
    if (!file)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    std::lock_guard lock(file->mutex);
    if (file->closed)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    if (!file->seek(offset, whence))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    const auto log_mode = [](const SceIoSeekMode whence) -> const char * {
        switch (whence) {
            STR_CASE(SCE_SEEK_SET);
            STR_CASE(SCE_SEEK_CUR);
            STR_CASE(SCE_SEEK_END);
        default:
            return "INVALID";
        }
    };

    LOG_TRACE_IF(log_file_op && log_file_seek, "{}: Seeking fd: {}, offset: {}, whence: {}", export_name, log_hex(fd), log_hex(offset), log_mode(whence));
    return file->tell();
}

SceOff tell_file(IOState &io, const SceUID fd, const char *export_name) {
    if (fd < 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EMFILE);

    const auto std_file = find_file(io, fd);

    if (!std_file) {
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    }

    std::lock_guard lock(std_file->mutex);
    if (std_file->closed)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    return std_file->tell();
}

int stat_file(IOState &io, const char *file, SceIoStat *statp, const fs::path &vita_fs_path, const char *export_name, const SceUID fd) {
    const std::lock_guard savedata_lock(io.savedata_mutex);
    assert(statp != nullptr);

    memset(statp, '\0', sizeof(SceIoStat));

    fs::path file_path = "";
    std::string mounted_path;
    if (fd == invalid_fd) {
        if (is_appmeta_path(file)) {
            file_path = appmeta_host_path(io, file);
            if (file_path.empty())
                return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
            mounted_path = file;
        } else {
            auto device = device::get_device(file);
            auto device_for_icase = device;
            if (device == VitaIoDevice::_INVALID) {
                LOG_ERROR("Cannot find device for path: {}", file);
                return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
            }

            const auto resolved_path = resolve_path(io, file, vita_fs_path);
            if (is_appmeta_path(resolved_path))
                return stat_file(io, resolved_path.c_str(), statp, vita_fs_path, export_name);
            device = device::get_device(resolved_path);
            const auto translated_path = translate_path(resolved_path.c_str(), device, io.device_paths);
            file_path = find_host_path(io, device::construct_emulated_path(device, translated_path, vita_fs_path, io.redirect_stdio));

            if (!fs::exists(file_path)) {
                if (io.case_isens_find_enabled) {
                    // Attempt a case-insensitive file search.
                    const auto original_file_path = file_path;
                    const auto cached_path = find_in_cache(io, string_utils::tolower(file_path.string()));
                    if (!cached_path.empty()) {
                        file_path = cached_path;
                        LOG_TRACE("Found cached filepath at {}", file_path);
                    } else {
                        const bool path_found = find_case_isens_path(io, device_for_icase, translated_path, file_path);
                        file_path = find_in_cache(io, string_utils::tolower(file_path.string()));
                        if (!file_path.empty() && path_found) {
                            LOG_TRACE("Found file on case-sensitive filesystem at {}", file_path);
                        } else {
                            LOG_ERROR("Missing file at {} (target path: {})", original_file_path, file);
                            return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                        }
                    }
                } else {
                    LOG_ERROR("Missing file at {} (target path: {})", file_path, file);
                    return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                }
            }
            LOG_TRACE_IF(log_file_op && log_file_stat, "{}: Statting file: {} ({})", export_name, file, device::construct_normalized_path(device, translated_path));
        }
    } else { // We have previously opened and defined the location
        const auto fd_file = find_file(io, fd);
        if (!fd_file)
            return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

        std::lock_guard lock(fd_file->mutex);
        if (fd_file->closed)
            return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
        if (is_appmeta_path(fd_file->get_vita_loc())) {
            file_path = appmeta_host_path(io, fd_file->get_vita_loc());
            if (file_path.empty())
                return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
            mounted_path = fd_file->get_vita_loc();
        } else {
            file_path = fd_file->get_system_location();
        }
        LOG_TRACE_IF(log_file_op && log_file_stat, "{}: Statting fd: {}", export_name, log_hex(fd));

        statp->st_attr = fd_file->get_file_mode();
    }

    std::uint64_t last_access_time_ticks;
    std::uint64_t creation_time_ticks;
    std::uint64_t last_modification_time_ticks;

#ifdef _WIN32
    struct _stati64 sb;
    if (_wstati64(file_path.generic_wstring().c_str(), &sb) < 0)
        return IO_ERROR_UNK();
#else
    struct stat64 sb;
    if (stat64(file_path.generic_string().c_str(), &sb) < 0)
        return IO_ERROR_UNK();
#endif

    last_access_time_ticks = RTC_OFFSET + static_cast<uint64_t>(sb.st_atime) * VITA_CLOCKS_PER_SEC;
    creation_time_ticks = RTC_OFFSET + static_cast<uint64_t>(sb.st_ctime) * VITA_CLOCKS_PER_SEC;
    last_modification_time_ticks = RTC_OFFSET + static_cast<uint64_t>(sb.st_mtime) * VITA_CLOCKS_PER_SEC;

#ifndef _WIN32
#undef st_atime
#undef st_mtime
#undef st_ctime
#endif

    // report regular files as readable but not executable
    statp->st_mode = SCE_S_IRUSR | SCE_S_IRGRP | SCE_S_IROTH;

    if (fs::is_regular_file(file_path)) {
        statp->st_size = fs::file_size(file_path);
        if (!mounted_path.empty()) {
            const auto resolved_layer = game_data_layer(io, mounted_path.c_str(), file_path);
            if (!resolved_layer)
                return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
            if (resolved_layer->layer) {
                try {
                    const auto *entry = resolved_layer->layer->find(resolved_layer->relative);
                    if (!entry)
                        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                    statp->st_size = entry->size;
                } catch (const std::runtime_error &error) {
                    LOG_ERROR("Cannot stat PFS file '{}': {}", mounted_path, error.what());
                    return IO_ERROR(SCE_ERROR_ERRNO_EIO);
                }
            }
        }
        statp->st_attr = SCE_SO_IFREG;
        statp->st_mode |= SCE_S_IFREG;
    }
    if (fs::is_directory(file_path)) {
        statp->st_attr = SCE_SO_IFDIR;
        statp->st_mode |= SCE_S_IFDIR | SCE_S_IXUSR | SCE_S_IXGRP | SCE_S_IXOTH;
    }

    __RtcTicksToPspTime(&statp->st_atime, last_access_time_ticks);
    __RtcTicksToPspTime(&statp->st_mtime, last_modification_time_ticks);
    __RtcTicksToPspTime(&statp->st_ctime, creation_time_ticks);

    return 0;
}

int stat_file_by_fd(IOState &io, const SceUID fd, SceIoStat *statp, const fs::path &vita_fs_path, const char *export_name) {
    assert(statp != nullptr);
    memset(statp, '\0', sizeof(SceIoStat));

    if (fd == invalid_fd)
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    return stat_file(io, nullptr, statp, vita_fs_path, export_name, fd);
}

int close_file(IOState &io, const SceUID fd, const char *export_name) {
    if (fd < 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EMFILE);

    LOG_TRACE_IF(log_file_op, "{}: Closing file fd: {}", export_name, log_hex(fd));

    StdFiles::node_type file;
    {
        std::lock_guard lock(io.fd_mutex);
        io.tty_files.erase(fd);
        file = io.std_files.extract(fd);
    }

    if (!file.empty()) {
        const auto &opened_file = file.mapped();
        std::lock_guard lock(opened_file->mutex);
        opened_file->closed = true;
        if (opened_file->pending_async_reads == 0 && opened_file->close() != 0) {
            LOG_ERROR("Failed to close file fd {}: host errno {}", log_hex(fd), errno);
            return IO_ERROR(SCE_ERROR_ERRNO_EIO);
        }
    }

    return 0;
}

int remove_file(IOState &io, const char *file, const fs::path &vita_fs_path, const char *export_name) {
    if (is_appmeta_path(file))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    auto device = device::get_device(file);
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", file);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto resolved_path = resolve_path(io, file, vita_fs_path, true);
    if (is_appmeta_path(resolved_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    device = device::get_device(resolved_path);
    const auto translated_path = translate_path(resolved_path.c_str(), device, io.device_paths);
    if (translated_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", translated_path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto emulated_path = find_host_path(io, device::construct_emulated_path(device, translated_path, vita_fs_path, io.redirect_stdio));
    if (overlaps_savedata_mount(io, emulated_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    if (!fs::exists(emulated_path) || fs::is_directory(emulated_path)) {
        LOG_ERROR("File does not exist at path: {} (target path: {})", emulated_path, file);
    }

    LOG_TRACE_IF(log_file_op, "{}: Removing file {} ({})", export_name, file, device::construct_normalized_path(device, translated_path));

    std::error_code error_code{};
    const auto res = fs::remove(emulated_path, error_code);

    if (!res || error_code.value()) {
        LOG_ERROR("Cannot remove file: {} ({})", file, device::construct_normalized_path(device, translated_path));
        LOG_ERROR("Error code: {} ({})", error_code.value(), error_code.message());
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    return 0;
}

int rename(IOState &io, const char *old_name, const char *new_name, const fs::path &vita_fs_path, const char *export_name) {
    if (is_appmeta_path(old_name) || is_appmeta_path(new_name))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    auto old_device = device::get_device(old_name);
    if (old_device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", old_name);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    auto new_device = device::get_device(new_name);
    if (new_device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", new_name);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto resolved_old_path = resolve_path(io, old_name, vita_fs_path, true);
    if (is_appmeta_path(resolved_old_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    old_device = device::get_device(resolved_old_path);
    const auto translated_old_path = translate_path(resolved_old_path.c_str(), old_device, io.device_paths);
    if (translated_old_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", old_name);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto resolved_new_path = resolve_path(io, new_name, vita_fs_path, true);
    if (is_appmeta_path(resolved_new_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    new_device = device::get_device(resolved_new_path);
    const auto translated_new_path = translate_path(resolved_new_path.c_str(), new_device, io.device_paths);
    if (translated_new_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", new_name);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto emulated_old_path = device::construct_emulated_path(old_device, translated_old_path, vita_fs_path, io.redirect_stdio);
    if (overlaps_savedata_mount(io, emulated_old_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    if (!fs::exists(emulated_old_path)) {
        LOG_ERROR("File does not exist at path: {} (target path: {})", emulated_old_path, old_name);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto emulated_new_path = device::construct_emulated_path(new_device, translated_new_path, vita_fs_path, io.redirect_stdio);
    if (overlaps_savedata_mount(io, emulated_new_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    LOG_TRACE_IF(log_file_op, "{}: Renaming file {} to {} ({} to {})", export_name, old_name, new_name, emulated_old_path, emulated_new_path);

    std::error_code error_code{};
    fs::rename(emulated_old_path, emulated_new_path, error_code);

    if (error_code.value()) {
        LOG_ERROR("Cannot rename file: {} to {} ({} to {})", old_name, new_name, emulated_old_path, emulated_new_path);
        LOG_ERROR("Error code: {} ({})", error_code.value(), error_code.message());
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    return 0;
}

SceUID open_dir(IOState &io, const char *path, const fs::path &vita_fs_path, const char *export_name) {
    const std::lock_guard savedata_lock(io.savedata_mutex);
    const auto candidates = is_appmeta_path(path) ? std::vector<std::string>{ path } : resolve_overlay_paths(io, path, vita_fs_path, false, 0, 0xFF, true);
    std::vector<DirPtr> opened;
    fs::path dir_path;
    std::string mounted_path;
    std::shared_ptr<void> mount_lease;
    auto mount_scope = pfsmgr::MountScope::Application;
    for (const auto &candidate : candidates) {
        std::vector<fs::path> host_paths;
        if (is_appmeta_path(candidate)) {
            std::lock_guard lock(io.appmeta_mutex);
            const auto mount = io.pfs_mounts.get_directory(mounted_path_key(io, candidate));
            if (mount) {
                mount_lease = mount->lease;
                mount_scope = mount->scope;
                mounted_path = candidate;
            }
        }
        if (is_appmeta_path(candidate)) {
            host_paths = appmeta_host_paths(io, candidate.c_str());
        } else {
            const auto unresolved = expand_unresolved_path(io, candidate.c_str(), vita_fs_path);
            if (!unresolved.empty())
                host_paths.push_back(find_host_path(io, unresolved));
        }
        for (const auto &candidate_path : host_paths) {
            if (!fs::exists(candidate_path))
                continue;
            if (!fs::is_directory(candidate_path)) {
                if (opened.empty()) {
                    LOG_ERROR("Not a directory: {} (target path: {})", candidate_path, path);
                    return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
                }
                break;
            }
            auto dir = create_shared_dir(candidate_path);
            if (!dir) {
                const std::error_code error(errno, std::generic_category());
                LOG_ERROR("Failed to open directory at: {} (target path: {}): {}", candidate_path, path, error.message());
                return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
            }
            if (opened.empty())
                dir_path = candidate_path;
            opened.push_back(std::move(dir));
        }
    }
    if (opened.empty()) {
        LOG_ERROR("Directory does not exist (target path: {})", path);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    std::string normalized = path;
    if (!is_appmeta_path(path)) {
        auto device = device::get_device(path);
        const auto translated_path = translate_path(path, device, io.device_paths);
        normalized = device::construct_normalized_path(device, translated_path);
    }
    auto dir = std::make_shared<DirStats>(mounted_path.empty() ? path : mounted_path.c_str(), normalized, dir_path, std::move(opened));
    dir->hold_mount(std::move(mount_lease));
    dir->set_mount_scope(mount_scope);
    SceUID fd;
    {
        std::lock_guard lock(io.fd_mutex);
        fd = io.next_fd++;
        io.dir_entries.emplace(fd, std::move(dir));
    }

    LOG_TRACE_IF(log_file_op, "{}: Opening dir {} ({}), fd: {}", export_name, path, normalized, log_hex(fd));

    return fd;
}

SceUID read_dir(IOState &io, const SceUID fd, SceIoDirent *dent, const fs::path &vita_fs_path, const char *export_name) {
    const std::lock_guard savedata_lock(io.savedata_mutex);
    assert(dent != nullptr);

    memset(dent->d_name, '\0', sizeof(dent->d_name));

    std::shared_ptr<DirStats> dir;
    {
        std::lock_guard lock(io.fd_mutex);
        const auto entry = io.dir_entries.find(fd);
        if (entry != io.dir_entries.end())
            dir = entry->second;
    }
    // Refuse any fd that is not explicitly a directory
    if (!dir || !dir->is_directory())
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    while (true) {
        std::string name;
        {
            std::lock_guard lock(dir->mutex);
            do {
                const auto entry = dir->get_dir_ptr();
                if (!entry)
                    return 0;
                name = get_file_in_dir(entry);
            } while (name == "." || name == ".." || !dir->add_entry(string_utils::tolower(name)));
        }
        auto file_path = std::string(dir->get_vita_loc());
        if (!file_path.ends_with('/'))
            file_path += '/';
        file_path += name;
        if (is_appmeta_path(file_path) && appmeta_host_path(io, file_path.c_str()).empty())
            continue;
        dent->d_name[name.copy(dent->d_name, sizeof(dent->d_name) - 1)] = '\0';
        LOG_TRACE_IF(log_file_op, "{}: Reading entry {} of fd: {}", export_name, file_path, log_hex(fd));
        if (stat_file(io, file_path.c_str(), &dent->d_stat, vita_fs_path, export_name) < 0)
            return IO_ERROR(SCE_ERROR_ERRNO_EMFILE);
        return 1;
    }
}

int create_dir(IOState &io, const char *dir, int mode, const fs::path &vita_fs_path, const char *export_name, const bool recursive) {
    if (is_appmeta_path(dir))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    auto device = device::get_device(dir);
    const auto resolved_path = resolve_path(io, dir, vita_fs_path, true);
    if (is_appmeta_path(resolved_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    device = device::get_device(resolved_path);
    const auto translated_path = translate_path(resolved_path.c_str(), device, io.device_paths);
    if (translated_path.empty()) {
        LOG_ERROR("Failed to translate path: {}", dir);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto emulated_path = device::construct_emulated_path(device, translated_path, vita_fs_path, io.redirect_stdio);
    if (overlaps_savedata_mount(io, emulated_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    if (recursive)
        return fs::create_directories(emulated_path);
    if (fs::exists(emulated_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EEXIST);

    auto parent_path = emulated_path.parent_path();
    if (!emulated_path.has_filename())
        parent_path = parent_path.parent_path();
    if (!fs::exists(parent_path)) // Vita cannot recursively create directories
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);

    LOG_TRACE_IF(log_file_op, "{}: Creating new dir {} ({})", export_name, dir, device::construct_normalized_path(device, translated_path));

    if (!fs::create_directory(emulated_path)) {
        LOG_ERROR("Failed to create directory at {} (target path: {})", emulated_path, dir);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    return 0;
}

int close_dir(IOState &io, const SceUID fd, const char *export_name) {
    if (fd < 0)
        return IO_ERROR(SCE_ERROR_ERRNO_EMFILE);

    DirEntries::node_type dir;
    {
        std::lock_guard lock(io.fd_mutex);
        dir = io.dir_entries.extract(fd);
    }

    LOG_TRACE_IF(log_file_op, "{}: Closing dir fd: {}", export_name, log_hex(fd));

    if (dir.empty())
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);

    return 0;
}

int remove_dir(IOState &io, const char *dir, const fs::path &vita_fs_path, const char *export_name) {
    if (is_appmeta_path(dir))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    auto device = device::get_device(dir);
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", dir);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    const auto resolved_path = resolve_path(io, dir, vita_fs_path, true);
    if (is_appmeta_path(resolved_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    device = device::get_device(resolved_path);
    const auto translated_path = translate_path(resolved_path.c_str(), device, io.device_paths);
    if (translated_path.empty()) {
        LOG_ERROR("Cannot translate path: {}", dir);
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    LOG_TRACE_IF(log_file_op, "{}: Removing dir {} ({})", export_name, dir, device::construct_normalized_path(device, translated_path));

    const auto emulated_path = device::construct_emulated_path(device, translated_path, vita_fs_path, io.redirect_stdio);
    if (overlaps_savedata_mount(io, emulated_path))
        return IO_ERROR(SCE_ERROR_ERRNO_EBADFD);
    if (!fs::remove_all(emulated_path)) {
        LOG_ERROR("Cannot remove dir: {} ({})", dir, device::construct_normalized_path(device, translated_path));
        return IO_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    return 0;
}
