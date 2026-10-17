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

#include <regmgr/state.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace regmgr {

void reset_regmgr(RegMgrState &regmgr);
void init_regmgr(RegMgrState &regmgr, const fs::path &vita_fs_path);

// Returns failure on an invalid or inconsistent registry or an I/O failure. Both calls
// require the loaded registry to match disk. The three-argument overload can
// preserve the original as system.dreg.bak; an existing backup is never replaced.
// A backup contains private registry data and remains even if replacement fails.
// Failure to synchronize the parent directory can be reported after replacement;
// in that case the loaded account ID has already been updated.
std::optional<std::array<uint8_t, 8>> get_account_id(RegMgrState &regmgr);
bool set_account_id(RegMgrState &regmgr, const std::array<uint8_t, 8> &account_id);
bool set_account_id(RegMgrState &regmgr, const std::array<uint8_t, 8> &account_id, bool backup_original);

// Setters return false for invalid input, an inconsistent registry, or an I/O failure.
// After a directory synchronization failure, the replacement may already be visible.
// Getters and setters for binary values
void get_bin_value(RegMgrState &regmgr, const std::string &category, const std::string &name, void *buf, uint32_t bufSize);
bool set_bin_value(RegMgrState &regmgr, const std::string &category, const std::string &name, const void *buf, uint32_t bufSize);

// Getters and setters for int values
int32_t get_int_value(RegMgrState &regmgr, const std::string &category, const std::string &name);
void set_int_value(RegMgrState &regmgr, const std::string &category, const std::string &name, const int32_t value);

// Getters and setters for string values
std::string get_str_value(RegMgrState &regmgr, const std::string &category, const std::string &name);
bool set_str_value(RegMgrState &regmgr, const std::string &category, const std::string &name, const char *value, const uint32_t bufSize);

// Getters of category and name by id
std::pair<std::string, std::string> get_category_and_name_by_id(const int id, const std::string &export_name);

} // namespace regmgr
