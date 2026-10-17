// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <io/functions.h>
#include <io/io.h>
#include <io/state.h>
#include <io/vfs.h>
#include <kernel/types.h>
#include <pfsmgr/mounts.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>
#include <vfs/mounts.h>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <optional>
#include <utility>

namespace pfsmgr {

using fs_utils::is_plain_component;
constexpr int ADDCONT_ALREADY_OPEN = static_cast<int>(0x80010078u);

bool mount_theme_data(IOState &io, const fs::path &directory, std::string &mount_point, bool &is_protected) {
    mount_point.clear();
    is_protected = false;
    const auto content_id = fs_utils::path_to_utf8(directory.filename());
    std::error_code error;
    if (!is_plain_component(directory.parent_path().parent_path()) || !is_plain_component(directory.parent_path())
        || !is_plain_component(directory) || !fs::is_directory(directory, error) || error) {
        LOG_ERROR("Invalid theme directory '{}'", directory);
        return false;
    }
    try {
        if (!has_databases(directory, is_protected))
            return false;
        if (is_protected && (content_id.size() < 17 || content_id[6] != '-' || content_id[16] != '_')) {
            LOG_ERROR("Invalid protected theme content ID '{}'", content_id);
            return false;
        }
        const auto title_id = is_protected ? content_id.substr(7, 9) : std::string{};
        auto alias = allocate_mount_name("tm");
        const auto pfs_name = allocate_mount_name("PD");
        if (alias.empty() || pfs_name.empty())
            return false;
        std::lock_guard lock(io.appmeta_mutex);
        if (!io.pfs_mounts.mount_directory(pfs_name, directory, {}, directory.parent_path().parent_path() / "license" / title_id,
                title_id, Mode::ThemeData, is_protected ? content_id : std::string{}, MountScope::Shell))
            return false;
        try {
            if (!io.virtual_pfs_mounts.emplace(MountKey{ MountType::ThemeData, alias }, pfs_name).second) {
                if (!io.pfs_mounts.unmount_directory(pfs_name))
                    LOG_ERROR("Cannot roll back theme pseudo-drive '{}'", pfs_name);
                LOG_ERROR("Duplicate theme mount alias '{}'", alias);
                return false;
            }
        } catch (const std::exception &error) {
            if (!io.pfs_mounts.unmount_directory(pfs_name))
                LOG_ERROR("Cannot roll back theme pseudo-drive '{}'", pfs_name);
            LOG_ERROR("Cannot publish theme mount '{}': {}", alias, error.what());
            return false;
        }
        mount_point = std::move(alias);
        return true;
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot mount theme '{}': {}", directory, error.what());
        return false;
    }
}

bool unmount_theme_data(IOState &io, const std::string &mount_point) {
    std::lock_guard lock(io.appmeta_mutex);
    const auto alias = io.virtual_pfs_mounts.find({ MountType::ThemeData, mount_point });
    if (alias == io.virtual_pfs_mounts.end())
        return false;
    if (!io.pfs_mounts.unmount_directory(alias->second)) {
        LOG_ERROR("Missing theme pseudo-drive for '{}'", mount_point);
        return false;
    }
    io.virtual_pfs_mounts.erase(alias);
    return true;
}

void clear_shell_mounts(IOState &io) {
    {
        std::lock_guard lock(io.fd_mutex);
        std::erase_if(io.std_files, [](const auto &entry) { return entry.second->is_shell_file(); });
        std::erase_if(io.dir_entries, [](const auto &entry) { return entry.second->is_shell_directory(); });
    }
    std::lock_guard lock(io.appmeta_mutex);
    std::erase_if(io.virtual_pfs_mounts, [](const auto &entry) { return entry.first.type == MountType::ThemeData; });
    io.pfs_mounts.clear(MountScope::Shell);
}

void clear_application_mounts(IOState &io) {
    std::lock_guard lock(io.appmeta_mutex);
    io.pfs_mounts.clear(MountScope::Application);
}

int mount_addcont(IOState &io, const fs::path &vita_fs_path, const std::string &mount_point, const std::string &entitlement) {
    if (mount_point != "addcont0:") {
        LOG_WARN("Unsupported additional content mount point '{}'", mount_point);
        return SCE_ERROR_ERRNO_EOPNOTSUPP;
    }
    const auto valid_id = [](const std::string &id, size_t length) {
        return id.size() == length && std::all_of(id.begin(), id.end(), [](char c) {
            return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        });
    };
    if (!valid_id(io.addcont, 9) || !valid_id(entitlement, 16))
        return SCE_ERROR_ERRNO_EINVAL;
    std::lock_guard lock(io.appmeta_mutex);
    if (io.virtual_pfs_mounts.contains({ MountType::AddCont, entitlement }))
        return ADDCONT_ALREADY_OPEN;
    const auto ux0 = vita_fs_path / "ux0";
    const auto parent = ux0 / "addcont" / io.addcont;
    const auto source = parent / entitlement;
    std::error_code ec;
    if (!is_plain_component(ux0) || !is_plain_component(ux0 / "addcont") || !is_plain_component(parent)
        || !is_plain_component(source) || !fs::is_directory(source, ec) || ec)
        return SCE_ERROR_ERRNO_ENOENT;
    const auto canonical_source = fs::canonical(source, ec);
    if (ec)
        return SCE_ERROR_ERRNO_EIO;
    if (io.addcont_pfs_mount.empty())
        io.addcont_pfs_mount = allocate_mount_name("PD");
    if (io.addcont_pfs_mount.empty())
        return SCE_ERROR_ERRNO_ENOSPC;
    const auto name = io.addcont_pfs_mount + "/" + entitlement;
    try {
        if (!io.pfs_mounts.mount_directory(name, source, {}, ux0 / "license" / io.addcont, io.addcont,
                Mode::AcidDirectory, {}, MountScope::Application, true))
            return SCE_ERROR_ERRNO_EIO;
        try {
            if (!io.virtual_pfs_mounts.emplace(MountKey{ MountType::AddCont, entitlement }, name).second) {
                if (!io.pfs_mounts.unmount_directory(name))
                    LOG_ERROR("Cannot roll back additional content pseudo-drive '{}'", name);
                LOG_ERROR("Duplicate additional content mount alias '{}'", entitlement);
                return SCE_ERROR_ERRNO_EIO;
            }
        } catch (const std::exception &error) {
            if (!io.pfs_mounts.unmount_directory(name))
                LOG_ERROR("Cannot roll back additional content pseudo-drive '{}'", name);
            LOG_ERROR("Cannot publish additional content mount '{}': {}", entitlement, error.what());
            return SCE_ERROR_ERRNO_EIO;
        }
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot mount additional content '{}': {}", entitlement, error.what());
        return SCE_ERROR_ERRNO_EIO;
    }
    return 0;
}

int unmount_addcont(IOState &io, const std::string &mount_point, const std::string &entitlement) {
    if (mount_point != "addcont0:")
        return SCE_ERROR_ERRNO_EOPNOTSUPP;
    std::lock_guard lock(io.appmeta_mutex);
    const auto entry = io.virtual_pfs_mounts.find({ MountType::AddCont, entitlement });
    if (entry == io.virtual_pfs_mounts.end())
        return SCE_ERROR_ERRNO_ENOENT;
    if (!io.pfs_mounts.unmount_directory(entry->second, true)) {
        if (io.pfs_mounts.get_directory(entry->second))
            return SCE_ERROR_ERRNO_EBUSY;
        LOG_ERROR("Missing additional content mount for '{}'", entitlement);
        return SCE_ERROR_ERRNO_EIO;
    }
    io.virtual_pfs_mounts.erase(entry);
    return 0;
}

bool mount_readonly_appmeta(IOState &io, const fs::path &vita_fs_path, const std::string &title_id, std::string &mount_point) {
    mount_point.clear();
    if (title_id.empty() || title_id.size() > 32 || !std::all_of(title_id.begin(), title_id.end(), [](char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        })
        || string_utils::tolower(title_id) == "new")
        return false;
    const auto ux0 = vita_fs_path / "ux0";
    const auto appmeta = ux0 / "appmeta";
    const auto source = appmeta / title_id;
    std::error_code ec;
    if (!is_plain_component(ux0) || !is_plain_component(appmeta) || !is_plain_component(source) || !fs::is_directory(source, ec) || ec)
        return false;
    const auto canonical_source = fs::canonical(source, ec);
    if (ec)
        return false;
    auto name = allocate_mount_name("ad");
    const auto pfs_name = allocate_mount_name("PD");
    if (name.empty() || pfs_name.empty())
        return false;
    std::lock_guard lock(io.appmeta_mutex);
    bool protected_appmeta = false;
    if (!has_appmeta_pfs(source, protected_appmeta))
        return false;
    if (!(protected_appmeta
                ? io.pfs_mounts.mount_savedata_directory(pfs_name, source, std::nullopt, MountScope::Shell)
                : io.pfs_mounts.mount_directory(pfs_name, source, {}, {}, title_id, Mode::Mode20, {}, MountScope::Shell)))
        return false;
    try {
        if (!io.virtual_pfs_mounts.emplace(MountKey{ MountType::AppMeta, name }, pfs_name).second) {
            LOG_ERROR("Duplicate app metadata mount alias '{}'", name);
            if (!io.pfs_mounts.unmount_directory(pfs_name))
                LOG_ERROR("Cannot release rejected theme mount '{}'", pfs_name);
            return false;
        }
    } catch (const std::exception &error) {
        io.pfs_mounts.unmount_directory(pfs_name);
        LOG_ERROR("Cannot publish app metadata mount for '{}': {}", title_id, error.what());
        return false;
    }
    mount_point = std::move(name);
    return true;
}

bool unmount_appmeta(IOState &io, const std::string &mount_point) {
    if (!is_mount_name(mount_point, "ad"))
        return false;
    std::lock_guard lock(io.appmeta_mutex);
    const auto alias = io.virtual_pfs_mounts.find({ MountType::AppMeta, mount_point });
    if (alias == io.virtual_pfs_mounts.end())
        return false;
    if (!io.pfs_mounts.unmount_directory(alias->second)) {
        LOG_ERROR("Missing app metadata mount for '{}'", mount_point);
        return false;
    }
    io.virtual_pfs_mounts.erase(alias);
    return true;
}

bool mount_readonly_game_data(IOState &io, const fs::path &vita_fs_path, const std::string &title_id, bool require_patch, std::string &mount_point) {
    mount_point.clear();
    if (title_id.empty() || title_id.size() > 32 || !std::all_of(title_id.begin(), title_id.end(), [](char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        }))
        return false;
    const auto ux0 = vita_fs_path / "ux0";
    const auto app = ux0 / "app";
    const auto source = app / title_id;
    const auto patch_parent = ux0 / "patch";
    const auto patch = patch_parent / title_id;
    std::error_code ec;
    if (!is_plain_component(ux0) || !is_plain_component(app) || !is_plain_component(source) || !fs::is_directory(source, ec) || ec)
        return false;
    const auto canonical_source = fs::canonical(source, ec);
    if (ec)
        return false;
    const bool patch_exists = fs::exists(patch, ec);
    if (ec || (require_patch && !patch_exists))
        return false;
    fs::path canonical_patch;
    if (patch_exists) {
        if (!is_plain_component(patch_parent) || !is_plain_component(patch) || !fs::is_directory(patch, ec) || ec)
            return false;
        canonical_patch = fs::canonical(patch, ec);
        if (ec)
            return false;
    }
    auto name = allocate_mount_name("gp");
    const auto pfs_name = allocate_mount_name("PD");
    if (name.empty() || pfs_name.empty())
        return false;
    std::lock_guard lock(io.appmeta_mutex);
    const auto license_directory = ux0 / "license" / title_id;
    if (!io.pfs_mounts.mount_directory(pfs_name, source, patch_exists ? patch : fs::path{}, license_directory, title_id, Mode::GameData))
        return false;
    try {
        if (!io.virtual_pfs_mounts.emplace(MountKey{ MountType::GameData, name }, pfs_name).second) {
            LOG_ERROR("Duplicate game data mount alias '{}'", name);
            if (!io.pfs_mounts.unmount_directory(pfs_name))
                LOG_ERROR("Cannot release rejected theme mount '{}'", pfs_name);
            return false;
        }
    } catch (const std::exception &error) {
        if (!io.pfs_mounts.unmount_directory(pfs_name))
            LOG_ERROR("Cannot release rejected app metadata mount '{}'", pfs_name);
        LOG_ERROR("Cannot publish game data mount for '{}': {}", title_id, error.what());
        return false;
    }
    mount_point = std::move(name);
    return true;
}

bool unmount_game_data(IOState &io, const std::string &mount_point) {
    if (!is_mount_name(mount_point, "gp"))
        return false;
    std::lock_guard lock(io.appmeta_mutex);
    if (mount_point == io.app_game_base_mount || mount_point == io.app_game_patch_mount)
        return false;
    const auto alias = io.virtual_pfs_mounts.find({ MountType::GameData, mount_point });
    if (alias == io.virtual_pfs_mounts.end())
        return false;
    if (!io.pfs_mounts.unmount_directory(alias->second))
        return false;
    io.virtual_pfs_mounts.erase(alias);
    return true;
}

static bool prepare_copy_destination(const fs::path &destination, bool directory) {
    const auto status = fs::symlink_status(destination);
    if (!fs::exists(status))
        return true;
    if (!fs::is_regular_file(status) && !fs::is_directory(status)) {
        LOG_ERROR("Unsupported copy destination '{}'", destination);
        return false;
    }
    if (fs::is_directory(status) != directory)
        fs::remove_all(destination);
    return true;
}

static bool copy_metadata(const fs::path &source, const fs::path &destination, bool regenerate_ebootparam = false) {
    const auto status = fs::symlink_status(source);
    if (!fs::exists(status)) {
        LOG_ERROR("Required appmeta source is missing: '{}'", source);
        return false;
    }
    if (fs::is_symlink(status)) {
        LOG_ERROR("Symlinks are not supported in appmeta sources: '{}'", source);
        return false;
    }
    if (fs::is_regular_file(status)) {
        if (!prepare_copy_destination(destination, false))
            return false;
        fs::create_directories(destination.parent_path());
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
        return true;
    }
    if (!fs::is_directory(status)) {
        LOG_ERROR("Unsupported appmeta source '{}'", source);
        return false;
    }
    if (!prepare_copy_destination(destination, true))
        return false;
    fs::create_directories(destination);
    for (const auto &entry : fs::directory_iterator(source)) {
        if (regenerate_ebootparam && string_utils::tolower(fs_utils::path_to_utf8(entry.path().filename())) == "ebootparam.bin") {
            if (!fs::is_regular_file(entry.symlink_status())) {
                LOG_ERROR("Invalid existing ebootparam '{}'", entry.path());
                return false;
            }
            continue;
        }
        if (!copy_metadata(entry.path(), destination / entry.path().filename()))
            return false;
    }
    return true;
}

enum class CopySelection { AppmetaRoot,
    AppmetaRetail,
    CacheRoot,
    All,
    ThemeRoot };

static bool copy_mounted_data(IOState &io, const fs::path &vita_fs_path, const std::string &source,
    const fs::path &destination, CopySelection selection, const fs::path &patch_root, size_t depth = 0) {
    const char *context = selection == CopySelection::ThemeRoot ? "theme" : "appmeta";
    if (depth > 128) {
        LOG_ERROR("{} directory nesting is too deep: '{}'", context, source);
        return false;
    }
    const auto fd = open_dir(io, source.c_str(), vita_fs_path, context);
    if (fd < 0) {
        LOG_ERROR("Cannot open mounted {} directory '{}'", context, source);
        return false;
    }
    struct DirectoryGuard {
        IOState &io;
        SceUID fd;
        const char *context;
        ~DirectoryGuard() {
            if (fd >= 0 && close_dir(io, fd, context) < 0)
                LOG_WARN("Cannot close {} directory descriptor {}", context, fd);
        }
    } directory{ io, fd, context };
    bool prepared = false;
    const auto prepare_directory = [&] {
        if (!prepared) {
            if (!prepare_copy_destination(destination, true))
                return false;
            fs::create_directories(destination);
            prepared = true;
        }
        return true;
    };
    if (selection != CopySelection::AppmetaRetail && !prepare_directory())
        return false;
    bool found_required_file = false;
    SceIoDirent entry{};
    int result = 0;
    while ((result = read_dir(io, fd, &entry, vita_fs_path, context)) > 0) {
        const std::string name(entry.d_name);
        if (selection == CopySelection::AppmetaRoot && name != "param.sfo" && name != "param2.sfo"
            && name != "icon0.png" && name != "pic0.png" && name != "livearea" && name != "retail")
            continue;
        if (selection == CopySelection::CacheRoot && name != "icon0.png" && name != "pic0.png" && name != "livearea" && name != "retail")
            continue;
        if (selection == CopySelection::AppmetaRetail && name != "livearea")
            continue;
        const auto component = fs_utils::utf8_to_path(name);
        if (name.empty() || name == "." || name == ".." || name.find_first_of("\\/:") != std::string::npos
            || component.has_root_path() || component.filename() != component) {
            LOG_ERROR("Invalid mounted {} entry '{}'", context, name);
            return false;
        }
        const auto child_source = source + "/" + name;
        const auto child_destination = destination / component;
        if (!patch_root.empty()) {
            // Existing appmeta already contains prior updates; only apply files supplied by this patch.
            const auto resolved = expand_path(io, child_source.c_str(), vita_fs_path);
            if (resolved.empty()) {
                LOG_ERROR("Cannot resolve mounted {} entry '{}'", context, child_source);
                return false;
            }
            const auto relative = resolved.lexically_relative(patch_root);
            if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
                continue;
        }
        if (!prepare_directory())
            return false;
        const auto file_type = entry.d_stat.st_attr & SCE_SO_IFMT;
        if (selection == CopySelection::ThemeRoot && file_type == SCE_SO_IFDIR)
            continue;
        if (selection == CopySelection::ThemeRoot && name == "theme.xml" && file_type == SCE_SO_IFREG)
            found_required_file = true;
        if (selection == CopySelection::AppmetaRoot && name == "param.sfo") {
            if (file_type != SCE_SO_IFREG) {
                LOG_ERROR("Invalid mounted appmeta param.sfo");
                return false;
            }
            found_required_file = true;
        }
        if ((selection == CopySelection::AppmetaRoot || selection == CopySelection::CacheRoot) && name == "retail" && file_type != SCE_SO_IFDIR) {
            LOG_ERROR("Invalid retail metadata directory");
            return false;
        }
        if (file_type == SCE_SO_IFDIR) {
            if (!copy_mounted_data(io, vita_fs_path, child_source, child_destination,
                    (selection == CopySelection::AppmetaRoot || selection == CopySelection::CacheRoot) && name == "retail" ? CopySelection::AppmetaRetail : CopySelection::All, patch_root, depth + 1))
                return false;
        } else if (file_type == SCE_SO_IFREG) {
            std::vector<uint8_t> data;
            if (!read_file_to_buffer(io, child_source.c_str(), vita_fs_path, data, context)) {
                LOG_ERROR("Cannot read mounted {} file '{}'", context, child_source);
                return false;
            }
            if (!prepare_copy_destination(child_destination, false))
                return false;
            std::ofstream output(child_destination, std::ios::binary | std::ios::trunc);
            if (!output || !output.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()))) {
                LOG_ERROR("Cannot write {} file '{}'", context, child_destination);
                return false;
            }
            output.close();
            if (!output) {
                LOG_ERROR("Cannot finish writing {} file '{}'", context, child_destination);
                return false;
            }
        } else {
            LOG_ERROR("Unsupported mounted {} entry '{}'", context, child_source);
            return false;
        }
    }
    if (result < 0) {
        LOG_ERROR("Cannot read mounted {} directory '{}'", context, source);
        return false;
    }
    if (selection == CopySelection::AppmetaRoot && !found_required_file) {
        LOG_ERROR("Mounted game is missing sce_sys/param.sfo");
        return false;
    }
    if (selection == CopySelection::ThemeRoot && !found_required_file) {
        LOG_ERROR("Mounted theme is missing theme.xml");
        return false;
    }
    if (close_dir(io, fd, context) < 0) {
        LOG_ERROR("Cannot close mounted {} directory '{}'", context, source);
        return false;
    }
    directory.fd = -1;
    return true;
}

bool copy_appmeta_cache(IOState &io, const fs::path &vita_fs_path, const std::string &title_id, const fs::path &destination) {
    std::string mount_point;
    if (!mount_readonly_appmeta(io, vita_fs_path, title_id, mount_point)) {
        LOG_ERROR("Cannot mount appmeta for shell cache: {}", title_id);
        return false;
    }
    struct MountGuard {
        IOState &io;
        const std::string &point;
        bool mounted = true;
        ~MountGuard() {
            if (mounted && !unmount_appmeta(io, point))
                LOG_ERROR("Cannot unmount appmeta for shell cache: {}", point);
        }
    } mount{ io, mount_point };
    try {
        if (!copy_mounted_data(io, vita_fs_path, mount_point, destination, CopySelection::CacheRoot, {}))
            return false;
        if (!unmount_appmeta(io, mount_point)) {
            LOG_ERROR("Cannot unmount appmeta for shell cache: {}", mount_point);
            return false;
        }
        mount.mounted = false;
        return true;
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot copy appmeta to shell cache for '{}': {}", title_id, error.what());
        return false;
    }
}

bool copy_theme_data(IOState &io, const fs::path &vita_fs_path, const std::string &content_id, const fs::path &destination) {
    if (content_id.empty() || content_id == "." || content_id == ".." || content_id.find_first_of("\\/:") != std::string::npos) {
        LOG_ERROR("Invalid theme content ID '{}'", content_id);
        return false;
    }
    std::string mount_point;
    bool is_protected = false;
    if (!mount_theme_data(io, vita_fs_path / "ux0" / "theme" / fs_utils::utf8_to_path(content_id), mount_point, is_protected))
        return false;
    struct MountGuard {
        IOState &io;
        const std::string &point;
        bool mounted = true;
        ~MountGuard() {
            if (mounted && !unmount_theme_data(io, point))
                LOG_ERROR("Cannot unmount theme data '{}'", point);
        }
    } mount{ io, mount_point };
    try {
        if (!copy_mounted_data(io, vita_fs_path, mount_point, destination, CopySelection::ThemeRoot, {}))
            return false;
        if (!unmount_theme_data(io, mount_point)) {
            LOG_ERROR("Cannot unmount theme data: {}", mount_point);
            return false;
        }
        mount.mounted = false;
        return true;
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot copy theme '{}': {}", content_id, error.what());
        return false;
    }
}

static bool read_appmeta_self_header(IOState &io, const fs::path &vita_fs_path, const std::string &mount_point, std::vector<uint8_t> &result) {
    constexpr size_t fixed_header_size = 120;
    constexpr uint64_t max_header_size = 16 * 1024 * 1024;
    const auto path = mount_point + "/eboot.bin";
    const auto fd = open_file(io, path.c_str(), SCE_O_RDONLY, vita_fs_path, "appmeta");
    if (fd < 0) {
        LOG_ERROR("Cannot open mounted eboot.bin");
        return false;
    }
    struct FileGuard {
        IOState &io;
        SceUID fd;
        ~FileGuard() {
            if (fd >= 0 && close_file(io, fd, "appmeta") < 0)
                LOG_WARN("Cannot close mounted eboot.bin");
        }
    } file{ io, fd };
    const auto file_size = seek_file(fd, 0, SCE_SEEK_END, io, "appmeta");
    if (file_size < static_cast<SceOff>(fixed_header_size) || seek_file(fd, 0, SCE_SEEK_SET, io, "appmeta") < 0) {
        LOG_ERROR("Cannot determine mounted eboot.bin size");
        return false;
    }
    const auto read_exact = [&](uint8_t *data, size_t size) {
        size_t offset = 0;
        while (offset < size) {
            const auto count = static_cast<SceSize>(std::min<size_t>(size - offset, 1024 * 1024));
            const auto read = read_file(data + offset, io, fd, count, "appmeta");
            if (read <= 0) {
                LOG_ERROR("Cannot read complete mounted eboot.bin header");
                return false;
            }
            offset += static_cast<size_t>(read);
        }
        return true;
    };
    std::array<uint8_t, fixed_header_size> fixed{};
    if (!read_exact(fixed.data(), fixed.size()))
        return false;
    uint64_t header_size = 0;
    for (size_t i = 0; i < 8; ++i)
        header_size |= uint64_t(fixed[16 + i]) << (8 * i);
    if (header_size < fixed.size() || header_size > max_header_size || header_size > static_cast<uint64_t>(file_size)) {
        LOG_ERROR("Invalid mounted eboot.bin header size");
        return false;
    }
    std::vector<uint8_t> header(static_cast<size_t>(header_size));
    std::copy(fixed.begin(), fixed.end(), header.begin());
    if (!read_exact(header.data() + fixed.size(), header.size() - fixed.size()))
        return false;
    if (close_file(io, fd, "appmeta") < 0) {
        LOG_ERROR("Cannot close mounted eboot.bin");
        return false;
    }
    file.fd = -1;
    result = std::move(header);
    return true;
}

bool copy_appmeta(IOState &io, const fs::path &vita_fs_path, const std::string &title_id,
    const fs::path &destination, const fs::path &existing_appmeta, bool require_patch, std::vector<uint8_t> *self_header) {
    if (self_header)
        self_header->clear();
    std::string mount_point;
    if (!mount_readonly_game_data(io, vita_fs_path, title_id, require_patch, mount_point)) {
        LOG_ERROR("Cannot mount game data for appmeta: {}", title_id);
        return false;
    }
    struct MountGuard {
        IOState &io;
        const std::string &point;
        bool mounted = true;
        ~MountGuard() {
            if (mounted && !unmount_game_data(io, point))
                LOG_WARN("Cannot unmount appmeta game data '{}'", point);
        }
    } mount{ io, mount_point };
    try {
        if (!existing_appmeta.empty()) {
            bool protected_appmeta = false;
            if (!has_appmeta_pfs(existing_appmeta, protected_appmeta))
                return false;
            if (protected_appmeta) {
                if (fs::absolute(existing_appmeta).lexically_normal() != fs::absolute(vita_fs_path / "ux0" / "appmeta" / title_id).lexically_normal()) {
                    LOG_ERROR("Invalid protected appmeta source '{}'", existing_appmeta);
                    return false;
                }
                std::string old_mount;
                if (!mount_readonly_appmeta(io, vita_fs_path, title_id, old_mount)) {
                    LOG_ERROR("Cannot mount previous protected appmeta for '{}'", title_id);
                    return false;
                }
                struct PreviousMountGuard {
                    IOState &io;
                    const std::string &point;
                    bool mounted = true;
                    ~PreviousMountGuard() {
                        if (mounted && !unmount_appmeta(io, point))
                            LOG_ERROR("Cannot unmount previous appmeta '{}'", point);
                    }
                } previous{ io, old_mount };
                if (!copy_mounted_data(io, vita_fs_path, old_mount, destination, CopySelection::AppmetaRoot, {}))
                    return false;
                if (!unmount_appmeta(io, old_mount)) {
                    LOG_ERROR("Cannot unmount previous protected appmeta '{}'", old_mount);
                    return false;
                }
                previous.mounted = false;
            } else {
                if (!copy_metadata(existing_appmeta, destination, self_header != nullptr))
                    return false;
            }
        }
        const auto patch = vita_fs_path / "ux0" / "patch" / title_id;
        if (!copy_mounted_data(io, vita_fs_path, mount_point + "/sce_sys", destination, CopySelection::AppmetaRoot,
                existing_appmeta.empty() ? fs::path{} : fs::absolute(patch)))
            return false;
        std::vector<uint8_t> header;
        if (self_header && !read_appmeta_self_header(io, vita_fs_path, mount_point, header))
            return false;
        if (!unmount_game_data(io, mount_point)) {
            LOG_ERROR("Cannot unmount appmeta game data: {}", mount_point);
            return false;
        }
        mount.mounted = false;
        if (self_header)
            *self_header = std::move(header);
        return true;
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot copy appmeta for '{}': {}", title_id, error.what());
        return false;
    }
}

bool read_update_history(IOState &io, const fs::path &vita_fs_path, const std::string &app_path,
    const std::string &title_id, int language, std::vector<uint8_t> &xml) {
    xml.clear();
    if (app_path != "ux0:app/" + title_id && app_path != "ux0:/app/" + title_id) {
        LOG_WARN("Unsupported application path for update history: '{}'", app_path);
        return false;
    }
    std::string mount_point;
    if (!mount_readonly_game_data(io, vita_fs_path, title_id, false, mount_point)) {
        LOG_WARN("Cannot mount game data for update history of '{}'", title_id);
        return false;
    }
    struct MountGuard {
        IOState &io;
        const std::string &point;
        ~MountGuard() {
            if (!unmount_game_data(io, point))
                LOG_WARN("Failed to unmount update history source '{}'", point);
        }
    } mount{ io, mount_point };

    const auto directory = mount_point + "/sce_sys/changeinfo/";
    const auto localized = directory + fmt::format("changeinfo_{:02}.xml", language);
    int open_error = 0;
    bool read = read_file_to_buffer(io, localized.c_str(), vita_fs_path, xml, "update_history", 16 * 1024 * 1024, &open_error, true);
    if (!read && open_error == SCE_ERROR_ERRNO_ENOENT) {
        const auto fallback = directory + "changeinfo.xml";
        read = read_file_to_buffer(io, fallback.c_str(), vita_fs_path, xml, "update_history", 16 * 1024 * 1024, &open_error, true);
    }
    if (!read || xml.empty()) {
        LOG_WARN("Failed to read update history for '{}' (open error {})", title_id, open_error);
        return false;
    }
    return true;
}

bool read_manual_pages(IOState &io, const fs::path &vita_fs_path, const std::string &app_path,
    const std::string &title_id, int language, const std::function<bool(const std::vector<uint8_t> &, unsigned int)> &consume_page) {
    const bool mount_game = app_path == "ux0:app/" + title_id || app_path == "ux0:/app/" + title_id;
    std::string mount_point;
    if (mount_game && !mount_readonly_game_data(io, vita_fs_path, title_id, false, mount_point)) {
        LOG_ERROR("Cannot mount manual for '{}'", title_id);
        return false;
    }
    struct MountGuard {
        IOState &io;
        const std::string &point;
        ~MountGuard() {
            if (!point.empty() && !unmount_game_data(io, point))
                LOG_WARN("Cannot unmount manual source '{}'", point);
        }
    } mount{ io, mount_point };

    const auto read_page = [&](const fs::path &relative, std::vector<uint8_t> &data) {
        // Read manual image from file buffer and check if it exists
        if (mount_game) {
            int open_error = 0;
            const auto path = mount_point + "/" + relative.generic_string();
            if (read_file_to_buffer(io, path.c_str(), vita_fs_path, data, "manual", std::numeric_limits<int>::max(), &open_error, true))
                return 0;
            if (open_error == SCE_ERROR_ERRNO_ENOENT) {
                try {
                    const auto host = expand_path(io, path.c_str(), vita_fs_path);
                    if (host.empty())
                        return SCE_ERROR_ERRNO_ENOENT;
                    std::error_code error;
                    const auto status = fs::symlink_status(host, error);
                    if (error == std::errc::no_such_file_or_directory || (!error && !fs::exists(status)))
                        return SCE_ERROR_ERRNO_ENOENT;
                } catch (const fs::filesystem_error &error) {
                    LOG_ERROR("Cannot inspect manual page '{}': {}", path, error.what());
                }
            }
            LOG_ERROR("Cannot open or read mounted manual page '{}'", path);
            return SCE_ERROR_ERRNO_EIO;
        }
        const auto path = vfs::resolve_app_path(vita_fs_path, app_path, relative, title_id);
        std::error_code error;
        const auto status = fs::status(path, error);
        if (error == std::errc::no_such_file_or_directory || (!error && !fs::exists(status)))
            return SCE_ERROR_ERRNO_ENOENT;
        if (error || !fs::is_regular_file(status) || !fs_utils::read_data(path, data) || data.size() > std::numeric_limits<int>::max()) {
            LOG_ERROR("Cannot read manual page '{}'", path);
            return SCE_ERROR_ERRNO_EIO;
        }
        return 0;
    };

    fs::path base = "sce_sys/manual";
    auto localized = base / fmt::format("{:02}", language);
    std::vector<uint8_t> data;
    auto result = read_page(localized / "001.png", data);
    if (result == 0)
        base = std::move(localized);
    else if (result == SCE_ERROR_ERRNO_ENOENT)
        result = read_page(base / "001.png", data);
    if (result != 0) {
        LOG_ERROR("Cannot read first manual page for '{}' (error {})", title_id, result);
        return false;
    }
    if (!consume_page(data, 1))
        return false;
    for (unsigned int page = 2; page <= 999; ++page) {
        // Get manual page path
        result = read_page(base / fmt::format("{:03}.png", page), data);
        if (result == SCE_ERROR_ERRNO_ENOENT)
            return true;
        if (result != 0) {
            LOG_ERROR("Cannot read manual page {} for '{}' (error {})", page, title_id, result);
            return false;
        }
        if (!consume_page(data, page))
            return false;
    }
    return true;
}

bool mount_application_game_data(IOState &io, const fs::path &vita_fs_path) {
    if (io.app_path != "ux0:app/" + io.title_id || io.title_id.empty())
        return true;
    if (io.title_id.size() > 32 || !std::all_of(io.title_id.begin(), io.title_id.end(), [](unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        })) {
        LOG_ERROR("Cannot mount application with invalid title ID '{}'.", io.title_id);
        return false;
    }
    if (!io.app_game_base_mount.empty()) {
        LOG_ERROR("Application game data is already mounted for '{}'", io.title_id);
        return false;
    }
    std::string mount;
    if (!mount_readonly_game_data(io, vita_fs_path, io.title_id, false, mount)) {
        LOG_ERROR("Cannot mount application game data for '{}'", io.title_id);
        return false;
    }
    auto patch_mount = allocate_mount_name("gp");
    const auto patch_pfs_mount = allocate_mount_name("PD");
    if (patch_mount.empty() || patch_pfs_mount.empty()) {
        LOG_ERROR("Cannot allocate patch mount for '{}'", io.title_id);
        if (!unmount_game_data(io, mount))
            LOG_ERROR("Cannot release rejected application game data mount '{}'", mount);
        return false;
    }
    std::unique_lock lock(io.appmeta_mutex);
    const auto base_alias = io.virtual_pfs_mounts.find({ MountType::GameData, mount });
    const auto base = base_alias == io.virtual_pfs_mounts.end() ? std::nullopt : io.pfs_mounts.get_directory(base_alias->second);
    if (!base) {
        LOG_ERROR("Missing application game mount '{}'", mount);
        lock.unlock();
        if (!unmount_game_data(io, mount))
            LOG_ERROR("Cannot release rejected application game data mount '{}'", mount);
        return false;
    }
    if (!base->patch.empty()) {
        const auto base_name = base_alias->second;
        try {
            const auto [alias, inserted] = io.virtual_pfs_mounts.emplace(MountKey{ MountType::GameData, patch_mount }, patch_pfs_mount);
            if (!inserted) {
                LOG_ERROR("Duplicate application patch alias '{}'", patch_mount);
                lock.unlock();
                if (!unmount_game_data(io, mount))
                    LOG_ERROR("Cannot release rejected application game data mount '{}'", mount);
                return false;
            }
            bool split = false;
            try {
                split = io.pfs_mounts.split_patch(base_name, patch_pfs_mount);
            } catch (const std::exception &error) {
                LOG_ERROR("Cannot separate application patch for '{}': {}", io.title_id, error.what());
            }
            if (!split) {
                LOG_ERROR("Cannot separate application patch for '{}'", io.title_id);
                io.virtual_pfs_mounts.erase(alias);
                lock.unlock();
                if (!unmount_game_data(io, mount))
                    LOG_ERROR("Cannot release rejected application game data mount '{}'", mount);
                return false;
            }
            io.app_game_patch_mount = std::move(patch_mount);
        } catch (const std::exception &error) {
            LOG_ERROR("Cannot mount application patch for '{}': {}", io.title_id, error.what());
            lock.unlock();
            if (!unmount_game_data(io, mount))
                LOG_ERROR("Cannot release rejected application game data mount '{}'", mount);
            return false;
        }
    }
    io.app_game_base_mount = std::move(mount);
    LOG_INFO("Mounted application game layers for '{}'", io.title_id);
    return true;
}

} // namespace pfsmgr