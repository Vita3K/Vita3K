// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <util/exit_code.h>
#include <util/fs.h>

#include <miniz.h>

#include <memory>
#include <optional>
#include <vector>

struct GuiState;
struct EmuEnvState;

typedef std::shared_ptr<mz_zip_archive> ZipPtr;

inline void delete_zip(mz_zip_archive *zip) {
    mz_zip_reader_end(zip);
    delete zip;
}

struct ArchiveContents {
    std::optional<float> count;
    std::optional<float> current;
    std::optional<float> progress;
};

struct ContentInfo {
    std::string title;
    std::string title_id;
    std::string category;
    std::string content_id;
    std::string path;
    bool state = false;
};

bool handle_events(EmuEnvState &emuenv, GuiState &gui);

std::vector<ContentInfo> install_archive(EmuEnvState &emuenv, GuiState *gui, const fs::path &archive_path, const std::function<void(ArchiveContents)> &progress_callback = nullptr);
uint32_t install_contents(EmuEnvState &emuenv, GuiState *gui, const fs::path &path);

ExitCode load_app(int32_t &main_module_id, EmuEnvState &emuenv);
ExitCode run_app(EmuEnvState &emuenv, int32_t main_module_id);
