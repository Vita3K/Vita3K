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

#include <packages/functions.h>
#include <packages/sfo.h>
#include <pfsmgr/functions.h>
#include <pfsmgr/mounts.h>
#include <util/log.h>
#include <vfs/mounts.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <mutex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <stdio.h>
#endif

static std::error_code publish_without_replacement(const fs::path &source, const fs::path &destination) {
#ifdef _WIN32
    if (MoveFileW(source.c_str(), destination.c_str()))
        return {};
    return { static_cast<int>(GetLastError()), std::system_category() };
#elif defined(__linux__) && defined(SYS_renameat2)
    if (syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD, destination.c_str(), RENAME_NOREPLACE) == 0)
        return {};
    return { errno, std::generic_category() };
#elif defined(__APPLE__)
    if (renamex_np(source.c_str(), destination.c_str(), RENAME_EXCL) == 0)
        return {};
    return { errno, std::generic_category() };
#else
    return std::make_error_code(std::errc::operation_not_supported);
#endif
}

static bool validate_sfo_file(const fs::path &path) {
    if (!fs::is_regular_file(fs::symlink_status(path)) || fs::file_size(path) > 32768) {
        LOG_ERROR("Missing or oversized appmeta param.sfo: {}", path);
        return false;
    }
    return true;
}

bool create_protected_appmeta(const fs::path &source, const fs::path &destination, const packages::Ebootparam &ebootparam) {
    constexpr uintmax_t max_file_size = 64 * 1024 * 1024;
    fs::path staging;
    bool success = false;
    try {
        success = [&]() -> bool {
            if (!fs::is_directory(fs::symlink_status(source)) || !fs::is_directory(fs::symlink_status(destination.parent_path()))
                || destination.filename().empty() || fs::exists(fs::symlink_status(destination))) {
                LOG_ERROR("Invalid source or destination for protected appmeta: {} -> {}", source, destination);
                return false;
            }
            const auto source_root = fs::canonical(source);
            const auto parent = fs::canonical(destination.parent_path());
            const auto relative_parent = parent.lexically_relative(source_root);
            if (relative_parent == "." || (!relative_parent.empty() && !relative_parent.is_absolute() && *relative_parent.begin() != "..")) {
                LOG_ERROR("Protected appmeta destination is inside its source: {}", destination);
                return false;
            }
            if (!validate_sfo_file(source / "param.sfo"))
                return false;

            static std::atomic_uint64_t sequence{ 0 };
            for (int attempt = 0; attempt < 128; ++attempt) {
                const auto candidate = parent / (".vita3k-appmeta-pfs-" + std::to_string(sequence++));
                if (fs::create_directory(candidate)) {
                    staging = candidate;
                    break;
                }
            }
            if (staging.empty()) {
                LOG_ERROR("Cannot reserve protected appmeta staging directory in {}", parent);
                return false;
            }

            const auto pfs = staging / "new";
            pfsmgr::create_initial_appmeta(pfs);
            {
                pfsmgr::PfsSession session(pfs);
                for (fs::recursive_directory_iterator it(source_root), end; it != end; ++it) {
                    if (it.depth() > 128) {
                        LOG_ERROR("Appmeta directory nesting is too deep: {}", it->path());
                        return false;
                    }
                    const auto relative = it->path().lexically_relative(source_root);
                    const auto first = relative.begin()->string();
                    std::string first_lower = first;
                    std::transform(first_lower.begin(), first_lower.end(), first_lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                    if (first_lower == "sce_pfs" || first_lower == "sce_sys" || first_lower == "ebootparam.bin") {
                        LOG_ERROR("Appmeta source contains a reserved entry: {}", it->path());
                        return false;
                    }
                    const auto status = it->symlink_status();
                    if (fs::is_directory(status)) {
                        session.create_directories(relative);
                    } else if (fs::is_regular_file(status)) {
                        if (fs::file_size(it->path()) > max_file_size) {
                            LOG_ERROR("Appmeta source file is too large: {}", it->path());
                            return false;
                        }
                        std::vector<uint8_t> data;
                        if (!fs_utils::read_data(it->path(), data) || data.size() > max_file_size) {
                            LOG_ERROR("Cannot read appmeta source file: {}", it->path());
                            return false;
                        }
                        session.write(relative, 0, data, true, false);
                        const auto verified = session.read(relative);
                        if (!verified || *verified != data) {
                            LOG_ERROR("Cannot verify protected appmeta file: {}", relative);
                            return false;
                        }
                    } else {
                        LOG_ERROR("Unsupported appmeta source entry: {}", it->path());
                        return false;
                    }
                }
                session.write("ebootparam.bin", 0, ebootparam, true, false);
                const auto verified = session.read("ebootparam.bin");
                if (!verified || !std::equal(verified->begin(), verified->end(), ebootparam.begin(), ebootparam.end())) {
                    LOG_ERROR("Cannot verify protected appmeta ebootparam");
                    return false;
                }
            }
            const auto error = publish_without_replacement(pfs, destination);
            if (error) {
                LOG_ERROR("Cannot publish protected appmeta {} to {}: {}", pfs, destination, error.message());
                return false;
            }
            std::error_code cleanup_error;
            if (!fs::remove(staging, cleanup_error))
                LOG_WARN("Could not remove empty appmeta staging directory {}: {}", staging, cleanup_error.message());
            return true;
        }();
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot create protected appmeta {}: {}", destination, error.what());
    } catch (...) {
        LOG_ERROR("Cannot create protected appmeta {}: unknown exception", destination);
    }
    if (!success && !staging.empty()) {
        std::error_code cleanup_error;
        fs::remove_all(staging, cleanup_error);
        if (cleanup_error)
            LOG_ERROR("Cannot clean protected appmeta staging directory {}: {}", staging, cleanup_error.message());
    }
    return success;
}

static bool validate_patch(const fs::path &source, const std::string &title_id) {
    const auto path = source / "param.sfo";
    if (!validate_sfo_file(path))
        return false;
    std::vector<uint8_t> data;
    SfoFile file{};
    std::string actual_id, category;
    if (!fs_utils::read_data(path, data) || !sfo::load(file, data)
        || !sfo::get_data_by_key(actual_id, file, "TITLE_ID") || actual_id != title_id
        || !sfo::get_data_by_key(category, file, "CATEGORY") || !category.starts_with("gp")) {
        LOG_ERROR("Invalid patch appmeta param.sfo: {}", path);
        return false;
    }
    return true;
}

static std::mutex appmeta_mutex;

template <typename Prepare>
static bool publish_shell_cache(const fs::path &vita_fs_path, const std::string &title_id, Prepare &&prepare) {
    fs::path work;
    bool preserve_backup = false;
    bool success = false;
    try {
        success = [&]() -> bool {
            const auto ur0 = vita_fs_path / "ur0";
            const auto parent = ur0 / "appmeta";
            for (const auto &directory : { ur0, parent }) {
                if (!fs::exists(fs::symlink_status(directory)))
                    fs::create_directory(directory);
                if (!fs_utils::is_plain_component(directory) || !fs::is_directory(fs::symlink_status(directory))) {
                    LOG_ERROR("Invalid shell appmeta cache directory: {}", directory);
                    return false;
                }
            }
            const auto destination = parent / title_id;
            const auto status = fs::symlink_status(destination);
            const bool had_cache = fs::exists(status);
            if (had_cache && (!fs::is_directory(status) || !fs_utils::is_plain_component(destination))) {
                LOG_ERROR("Invalid shell appmeta cache: {}", destination);
                return false;
            }

            static std::atomic_uint64_t sequence{ 0 };
            for (int attempt = 0; attempt < 128; ++attempt) {
                const auto candidate = parent / (".vita3k-cache-" + title_id + "-" + std::to_string(sequence++));
                if (fs::create_directory(candidate)) {
                    work = candidate;
                    break;
                }
            }
            if (work.empty()) {
                LOG_ERROR("Cannot reserve shell appmeta cache staging directory in {}", parent);
                return false;
            }

            const auto prepared = work / "new";
            fs::create_directory(prepared);
            if (!prepare(prepared))
                return false;
            if (had_cache)
                fs::rename(destination, work / "old");
            const auto error = publish_without_replacement(prepared, destination);
            if (error) {
                if (had_cache) {
                    const auto restore_error = publish_without_replacement(work / "old", destination);
                    if (restore_error) {
                        preserve_backup = true;
                        LOG_ERROR("Could not restore shell appmeta cache for {}; original preserved at {}: {}", title_id, work / "old", restore_error.message());
                    }
                }
                LOG_ERROR("Cannot publish shell appmeta cache {} to {}: {}", prepared, destination, error.message());
                return false;
            }
            return true;
        }();
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot publish shell appmeta cache for {}: {}", title_id, error.what());
    } catch (...) {
        LOG_ERROR("Cannot publish shell appmeta cache for {}: unknown exception", title_id);
    }
    if (!success) {
        if (!preserve_backup && !work.empty()) {
            std::error_code cleanup_error;
            fs::remove_all(work, cleanup_error);
            if (cleanup_error)
                LOG_ERROR("Could not clean shell cache staging directory {}: {}", work, cleanup_error.message());
        }
        return false;
    }
    std::error_code cleanup_error;
    fs::remove_all(work, cleanup_error);
    if (cleanup_error)
        LOG_WARN("Could not clean shell cache staging directory {}: {}", work, cleanup_error.message());
    return true;
}

bool refresh_appmeta_cache(IOState &io, const fs::path &vita_fs_path, const std::string &title_id) {
    const std::lock_guard lock(appmeta_mutex);
    if (title_id.empty() || title_id.size() > 32 || !std::all_of(title_id.begin(), title_id.end(), [](unsigned char ch) {
            return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
        })) {
        LOG_ERROR("Cannot refresh shell appmeta cache for invalid title ID '{}'", title_id);
        return false;
    }
    try {
        return publish_shell_cache(vita_fs_path, title_id, [&](const fs::path &prepared) {
            if (!pfsmgr::copy_appmeta_cache(io, vita_fs_path, title_id, prepared)) {
                LOG_ERROR("Cannot copy mounted appmeta into shell cache for {}", title_id);
                return false;
            }
            return true;
        });
    } catch (const std::runtime_error &error) {
        LOG_ERROR("Cannot refresh shell appmeta cache for '{}': {}", title_id, error.what());
        return false;
    }
}

bool create_appmeta(IOState &io, const fs::path &vita_fs_path, const std::string &title_id, AppmetaInstallType type, AppmetaFormat format, AppmetaCache cache) {
    const std::lock_guard lock(appmeta_mutex);
    if (title_id.empty() || title_id.size() > 32 || !std::all_of(title_id.begin(), title_id.end(), [](unsigned char ch) {
            return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
        })) {
        LOG_ERROR("Appmeta: invalid title ID '{}'; metadata not created", title_id);
        return false;
    }

    const auto ux0 = vita_fs_path / "ux0";
    const auto app = ux0 / "app" / title_id;
    const auto source = app / "sce_sys";
    const auto patch = ux0 / "patch" / title_id;
    const auto patch_source = patch / "sce_sys";
    const auto destination = ux0 / "appmeta" / title_id;
    fs::path staging;
    fs::path protected_staging;
    fs::path backup;
    bool metadata_published = false;
    bool success = false;
    try {
        success = [&]() -> bool {
            const bool has_destination = fs::exists(fs::symlink_status(destination));
            if (has_destination && !fs::is_directory(fs::symlink_status(destination))) {
                LOG_ERROR("Cannot update invalid appmeta directory: {}", destination);
                return false;
            }
            if (has_destination && format == AppmetaFormat::Plain) {
                bool protected_destination = false;
                if (!pfsmgr::has_appmeta_pfs(destination, protected_destination)) {
                    LOG_ERROR("Cannot inspect appmeta protection: {}", destination);
                    return false;
                }
                if (protected_destination) {
                    LOG_ERROR("Cannot replace protected appmeta with plaintext metadata: {}", destination);
                    return false;
                }
            }
            const bool reuse_destination = type == AppmetaInstallType::Patch && has_destination;
            if (!reuse_destination) {
                if (!fs::is_directory(fs::symlink_status(app)) || !fs::is_directory(fs::symlink_status(source))) {
                    LOG_ERROR("Invalid appmeta source directory: {}", source);
                    return false;
                }
                if (!validate_sfo_file(source / "param.sfo"))
                    return false;
            }
            const bool has_patch = fs::exists(fs::symlink_status(patch));
            if (type == AppmetaInstallType::Patch && !has_patch) {
                LOG_ERROR("Installed patch is missing: {}", patch);
                return false;
            }
            if (has_patch) {
                if (!fs::is_directory(fs::symlink_status(patch)) || !fs::is_directory(fs::symlink_status(patch_source))) {
                    LOG_ERROR("Invalid patch metadata directory: {}", patch_source);
                    return false;
                }
                if (!validate_sfo_file(patch_source / "param.sfo"))
                    return false;
            }

            static std::atomic_uint64_t sequence{ 0 };
            for (int attempt = 0; attempt < 128; ++attempt) {
                const auto candidate = ux0 / (".vita3k-appmeta-" + title_id + "-" + std::to_string(sequence++));
                if (fs::create_directory(candidate)) {
                    staging = candidate;
                    break;
                }
            }
            if (staging.empty()) {
                LOG_ERROR("Cannot reserve appmeta staging directory in {}", ux0);
                return false;
            }

            std::vector<uint8_t> self_header;
            if (!pfsmgr::copy_appmeta(io, vita_fs_path, title_id, staging,
                    reuse_destination ? destination : fs::path{}, type == AppmetaInstallType::Patch,
                    format == AppmetaFormat::Protected ? &self_header : nullptr)) {
                LOG_ERROR("Cannot copy mounted game metadata for appmeta: {}", title_id);
                return false;
            }
            if (has_patch && !validate_patch(staging, title_id))
                return false;

            fs::path publication = staging;
            if (format == AppmetaFormat::Protected) {
                const auto ebootparam = packages::generate_ebootparam(self_header);
                if (!ebootparam) {
                    LOG_ERROR("Cannot generate appmeta ebootparam from mounted eboot.bin");
                    return false;
                }
                auto candidate = staging;
                candidate += "-pfs";
                if (!create_protected_appmeta(staging, candidate, *ebootparam))
                    return false;
                protected_staging = candidate;
                publication = candidate;
            }

            fs::create_directories(destination.parent_path());
            if (has_destination) {
                auto backup_candidate = staging;
                backup_candidate += "-backup";
                if (!fs::create_directory(backup_candidate)) {
                    LOG_ERROR("Cannot reserve appmeta backup directory: {}", backup_candidate);
                    return false;
                }
                backup = backup_candidate;
                fs::rename(destination, backup / "previous");
            }
            const auto error = publish_without_replacement(publication, destination);
            if (error) {
                if (has_destination) {
                    const auto restore_error = publish_without_replacement(backup / "previous", destination);
                    if (restore_error)
                        LOG_ERROR("Could not restore appmeta for {}; original preserved at {}: {}", title_id, backup / "previous", restore_error.message());
                }
                LOG_ERROR("Cannot publish appmeta {} to {}: {}", publication, destination, error.message());
                return false;
            }
            metadata_published = true;
            if (cache == AppmetaCache::Populate) {
                const auto &source = format == AppmetaFormat::Plain ? destination : staging;
                if (!publish_shell_cache(vita_fs_path, title_id, [&](const fs::path &prepared) {
                        for (const auto &relative : { fs::path("icon0.png"), fs::path("pic0.png"), fs::path("livearea"), fs::path("retail") / "livearea" }) {
                            const auto from = source / relative;
                            const auto entry_status = fs::symlink_status(from);
                            if (!fs::exists(entry_status))
                                continue;
                            if (fs::is_symlink(entry_status) || (!fs::is_regular_file(entry_status) && !fs::is_directory(entry_status))) {
                                LOG_ERROR("Invalid shell cache resource: {}", from);
                                return false;
                            }
                            fs::create_directories((prepared / relative).parent_path());
                            fs::copy(from, prepared / relative, fs::copy_options::recursive);
                        }
                        return true;
                    })) {
                    LOG_ERROR("Appmeta for {} was published, but its shell cache could not be updated", title_id);
                    return false;
                }
            }
            if (!protected_staging.empty()) {
                std::error_code cleanup_error;
                fs::remove_all(staging, cleanup_error);
                if (cleanup_error)
                    LOG_WARN("Could not clean plaintext appmeta staging directory {}: {}", staging, cleanup_error.message());
            }
            if (!backup.empty()) {
                std::error_code cleanup_error;
                fs::remove_all(backup, cleanup_error);
                if (cleanup_error)
                    LOG_ERROR("Could not clean appmeta backup directory {}: {}", backup, cleanup_error.message());
            }
            LOG_INFO("{} Appmeta created or updated for {} at {} using {}{}",
                format == AppmetaFormat::Protected ? "Protected" : "Plain", title_id, destination, reuse_destination ? "existing metadata" : "application metadata",
                has_patch ? " with separate patch overrides" : "");
            return true;
        }();
    } catch (const std::exception &error) {
        LOG_ERROR("Appmeta operation for {} failed{}: {}", title_id,
            metadata_published ? " after publishing metadata" : " before publishing metadata", error.what());
    }
    if (!success) {
        LOG_ERROR("Appmeta operation for {} failed {} publishing metadata", title_id, metadata_published ? "after" : "before");
        if (!staging.empty()) {
            std::error_code cleanup_error;
            fs::remove_all(staging, cleanup_error);
            if (cleanup_error)
                LOG_ERROR("Could not clean appmeta staging directory {}: {}", staging, cleanup_error.message());
        }
        if (!protected_staging.empty()) {
            std::error_code cleanup_error;
            fs::remove_all(protected_staging, cleanup_error);
            if (cleanup_error)
                LOG_ERROR("Could not clean protected appmeta staging directory {}: {}", protected_staging, cleanup_error.message());
        }
        if (!backup.empty()) {
            std::error_code cleanup_error;
            fs::remove(backup, cleanup_error);
            if (cleanup_error)
                LOG_WARN("Appmeta backup retained at {}: {}", backup, cleanup_error.message());
        }
        return false;
    }
    return true;
}
