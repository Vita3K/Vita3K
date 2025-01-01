// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

// TODO: remove
#include <util/fs.h>

#include <functional>
#include <string>

void install_pup(const fs::path &pref_path, const fs::path &pup_path, const std::function<void(uint32_t)> &progress_callback = nullptr);
