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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace updater {

enum class UpdateCheckMode {
    // User asked to check now, so always surface progress and the result.
    ManualInteractive,
    // Startup check that may open a dialog when an update is found.
    StartupPrompt,
    // Quiet startup check used only to cache update availability.
    StartupBackground,
};

enum class UpdateCheckStatus {
    Failed,
    UpToDate,
    UpdateAvailable,
    CurrentBuildNewerThanLatest,
    CustomBuildCanUpdate,
};

struct ChangelogEntry {
    // Version label shown for this changelog entry.
    std::string version;
    // Short summary line for this changelog entry.
    std::string title;
};

struct UpdateInfo {
    // Human-readable release name or tag.
    std::string version;
    // Numeric build identifier parsed from the release notes.
    std::uint64_t build_number = 0;
    // Browser URL for the release page the user can open.
    std::string release_url;
    // GitHub publication timestamp in ISO-8601 format.
    std::string published_at;
    // Release notes with updater metadata lines stripped out.
    std::string notes;
    // Optional changelog entries returned by the release payload.
    std::vector<ChangelogEntry> changelog;
};

struct UpdateCheckResult {
    UpdateCheckStatus status = UpdateCheckStatus::Failed;
    std::string message;
    // Parsed release metadata for dialogs and notifications.
    UpdateInfo info;
    // Display version string for the currently installed build.
    std::string current_display_version;
};

} // namespace updater
