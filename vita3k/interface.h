// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <memory>
#include <optional>

#include <app/functions.h>
#include <util/exit_code.h>
#include <util/fs.h>

#include <miniz.h>

struct GuiState;
struct HostState;

typedef std::shared_ptr<mz_zip_archive> ZipPtr;
typedef std::shared_ptr<mz_zip_reader_extract_iter_state> ZipFilePtr;

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

bool handle_events(HostState &host, GuiState &gui);

std::vector<ContentInfo> install_archive(HostState &host, GuiState *gui, const fs::path &archive_path, const std::function<void(ArchiveContents)> &progress_callback = nullptr);
uint32_t install_contents(HostState &host, GuiState *gui, const fs::path &path);

ExitCode load_app(Ptr<const void> &entry_point, HostState &host, const std::wstring &path);
ExitCode run_app(HostState &host, Ptr<const void> &entry_point);
