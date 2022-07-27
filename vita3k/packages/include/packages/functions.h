// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

/**
 * @file functions.h
 * @brief Software package (`.pkg`) and license handling functions
 */

#pragma once

#include <emuenv/state.h>

#include <functional>
#include <string>
#include <vector>

// TODO: remove
#include <util/fs.h>

struct SfoFile;

void install_pup(const std::wstring &pref_path, const std::string &pup_path, const std::function<void(uint32_t)> &progress_callback = nullptr);

bool create_license(EmuEnvState &emuenv, const std::string &zRIF);
bool copy_license(EmuEnvState &emuenv, const fs::path &license_path);
int32_t get_license_sku_flag(EmuEnvState &emuenv, const std::string &content_id);
