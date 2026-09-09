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

#include <updater/functions.h>

#include <util/archive_utils.h>
#include <util/log.h>

#include <fmt/format.h>

#include <cstdlib>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace stdfs = std::filesystem;

constexpr std::string_view k_backup_suffix = ".vita3k-old";
constexpr std::string_view k_staging_dir_name = ".vita3k-update";
constexpr std::string_view k_pending_marker_name = ".vita3k-update-pending";

// One replacement that already happened, kept so a failure halfway through can be undone.
struct AppliedFile {
    stdfs::path destination;
    // Empty when the update added a file that did not exist before.
    stdfs::path backup;
};

stdfs::path backup_path_for(const stdfs::path &destination) {
    stdfs::path backup = destination;
    backup += k_backup_suffix;
    return backup;
}

constexpr stdfs::perms k_exec_bits = stdfs::perms::owner_exec | stdfs::perms::group_exec | stdfs::perms::others_exec;

// The archive may not carry Unix permissions, and a Vita3K binary that lost its exec bit would not start again
void carry_over_exec_bits(const stdfs::perms previous, const stdfs::path &destination) {
    const stdfs::perms wanted = previous & k_exec_bits;
    if (wanted == stdfs::perms::none)
        return;

    std::error_code ec;
    stdfs::permissions(destination, wanted, stdfs::perm_options::add, ec);
}

bool install_appimage(std::span<const uint8_t> payload, const stdfs::path &appimage, std::string &error_message) {
    stdfs::path staged = appimage;
    staged += ".vita3k-new";

    std::error_code ec;
    {
        std::ofstream out(staged, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char *>(payload.data()), static_cast<std::streamsize>(payload.size()));
        if (!out) {
            out.close();
            stdfs::remove(staged, ec);
            error_message = fmt::format("Could not write the new AppImage to {}", staged.string());
            return false;
        }
    }

    // An AppImage that is not executable cannot be launched again
    constexpr stdfs::perms appimage_perms = stdfs::perms::owner_all | stdfs::perms::group_read | stdfs::perms::group_exec | stdfs::perms::others_read | stdfs::perms::others_exec;
    stdfs::permissions(staged, appimage_perms, ec);
    if (ec) {
        stdfs::remove(staged, ec);
        error_message = fmt::format("Could not make {} executable: {}", staged.string(), ec.message());
        return false;
    }

    // Renaming over the old AppImage is atomic, and the running one keeps the inode it was mounted from
    stdfs::rename(staged, appimage, ec);
    if (ec) {
        stdfs::remove(staged, ec);
        error_message = fmt::format("Could not replace {}: {}", appimage.string(), ec.message());
        return false;
    }

    LOG_INFO("Replaced the AppImage at {}", appimage.string());
    return true;
}

std::vector<stdfs::path> collect_staged_files(const stdfs::path &staging) {
    std::vector<stdfs::path> files;

    std::error_code iterate_error;
    for (stdfs::recursive_directory_iterator it(staging, iterate_error), end; it != end; it.increment(iterate_error)) {
        if (iterate_error)
            break;

        std::error_code entry_error;
        if (!it->is_regular_file(entry_error) || entry_error)
            continue;

        stdfs::path relative = stdfs::relative(it->path(), staging, entry_error);
        if (entry_error || relative.empty())
            continue;

        files.push_back(std::move(relative));
    }

    return files;
}

void undo_applied_files(const std::vector<AppliedFile> &applied) {
    std::error_code ec;
    for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
        stdfs::remove(it->destination, ec);
        if (!it->backup.empty())
            stdfs::rename(it->backup, it->destination, ec);
    }
}

} // namespace

namespace updater {

std::string appimage_path() {
#if defined(__linux__) && !defined(__ANDROID__)
    const char *path = std::getenv("APPIMAGE");
    if (path && *path)
        return path;
#endif
    return {};
}

std::string release_asset_name() {
#if defined(_WIN32)
#if defined(_M_ARM64) || defined(__aarch64__)
    return "windows-arm64-latest.7z";
#else
    return "windows-latest.7z";
#endif
#elif defined(__linux__) && !defined(__ANDROID__)
#if defined(__aarch64__)
    return appimage_path().empty() ? "ubuntu-aarch64-latest.7z" : "Vita3K-aarch64.AppImage";
#else
    return appimage_path().empty() ? "ubuntu-latest.7z" : "Vita3K-x86_64.AppImage";
#endif
#else
    return {};
#endif
}

std::string release_asset_url() {
    const std::string asset = release_asset_name();
    if (asset.empty())
        return {};

    return fmt::format("https://github.com/Vita3K/Vita3K/releases/download/continuous/{}", asset);
}

bool can_self_update() {
    return !release_asset_name().empty();
}

bool install_update(std::span<const uint8_t> archive_data, const stdfs::path &install_dir, std::string &error_message) {
    const std::string appimage = appimage_path();
    if (!appimage.empty())
        return install_appimage(archive_data, appimage, error_message);

    const stdfs::path staging = install_dir / k_staging_dir_name;

    std::error_code ec;
    stdfs::remove_all(staging, ec);
    stdfs::create_directories(staging, ec);
    if (ec) {
        error_message = fmt::format("Could not create the update staging directory {}: {}", staging.string(), ec.message());
        return false;
    }

    if (!archive_utils::extract_7z(archive_data, staging)) {
        stdfs::remove_all(staging, ec);
        error_message = "The downloaded update archive could not be extracted.";
        return false;
    }

    const std::vector<stdfs::path> staged_files = collect_staged_files(staging);
    if (staged_files.empty()) {
        stdfs::remove_all(staging, ec);
        error_message = "The downloaded update archive did not contain any file.";
        return false;
    }

    std::vector<AppliedFile> applied;
    applied.reserve(staged_files.size());

    for (const stdfs::path &relative : staged_files) {
        const stdfs::path source = staging / relative;
        const stdfs::path destination = install_dir / relative;

        stdfs::create_directories(destination.parent_path(), ec);

        AppliedFile entry{ destination, {} };
        stdfs::perms previous_perms = stdfs::perms::none;
        if (stdfs::exists(destination, ec)) {
            previous_perms = stdfs::status(destination, ec).permissions();
            const stdfs::path backup = backup_path_for(destination);
            stdfs::remove(backup, ec);

            // Windows locks the running executable and its DLLs against writes, but still allows renaming them
            stdfs::rename(destination, backup, ec);
            if (ec) {
                error_message = fmt::format("Could not move {} out of the way: {}", destination.string(), ec.message());
                undo_applied_files(applied);
                return false;
            }

            entry.backup = backup;
        }

        stdfs::rename(source, destination, ec);
        if (ec) {
            error_message = fmt::format("Could not install {}: {}", destination.string(), ec.message());
            if (!entry.backup.empty())
                stdfs::rename(entry.backup, destination, ec);
            undo_applied_files(applied);
            return false;
        }

        carry_over_exec_bits(previous_perms, destination);
        applied.push_back(std::move(entry));
    }

    stdfs::remove_all(staging, ec);

    // Marks the backups for deletion so the next launch does not have to scan the install directory for nothing
    std::ofstream(install_dir / k_pending_marker_name).close();

    LOG_INFO("Installed {} updated file(s) in {}", applied.size(), install_dir.string());
    return true;
}

void remove_update_backups(const stdfs::path &install_dir) {
    const stdfs::path marker = install_dir / k_pending_marker_name;

    std::error_code ec;
    if (!stdfs::exists(marker, ec))
        return;

    stdfs::remove_all(install_dir / k_staging_dir_name, ec);

    std::vector<stdfs::path> backups;
    for (stdfs::recursive_directory_iterator it(install_dir, ec), end; it != end; it.increment(ec)) {
        if (ec)
            break;

        const std::string filename = it->path().filename().string();
        if (filename.size() > k_backup_suffix.size() && filename.ends_with(k_backup_suffix))
            backups.push_back(it->path());
    }

    for (const stdfs::path &backup : backups) {
        stdfs::remove(backup, ec);
        if (ec)
            LOG_WARN("Could not remove the leftover update backup {}", backup.string());
    }

    stdfs::remove(marker, ec);
}

} // namespace updater
