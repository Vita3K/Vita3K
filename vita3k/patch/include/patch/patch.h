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
#include <set>
#include <string>
#include <util/fs.h>
#include <vector>

struct Patch {
    uint8_t seg;
    uint32_t offset;
    std::vector<uint8_t> values;
};

struct PatchHeader {
    std::string titleid;
    std::string bin;
};

using Patches = std::vector<Patch>;

struct PatchEntry {
    std::vector<std::string> title_ids;
    std::string title;
    std::string name;
    std::string author;
    std::string note;
    std::string patch_ver;
    std::string app_ver;
    std::string app_elf;
    std::string nid;
    Patches patches;

    std::string key() const;
};

using PatchEntries = std::vector<PatchEntry>;
using EnabledPatches = std::set<std::string>;

Patches get_patches(fs::path &path, const std::string &titleid, const std::string &bin);
Patch parse_patch(const std::string &patch);

inline constexpr const char *PATCHLIST_XML_URL = "https://github.com/Vita3K/vita3k-patches/releases/download/latest/patchlist.xml";

fs::path patchlist_xml_path(const fs::path &patch_dir);
fs::path patch_config_path(const fs::path &patch_dir);

bool parse_patch_entries(const std::string &xml, PatchEntries &entries);
PatchEntries load_patch_entries(const fs::path &patch_dir);

EnabledPatches load_enabled_patches(const fs::path &patch_dir);
bool save_enabled_patches(const fs::path &patch_dir, const EnabledPatches &enabled);

bool patch_versions_equal(const std::string &a, const std::string &b);
bool patch_entry_matches(const PatchEntry &entry, const std::string &titleid, const std::string &bin, const std::string &app_ver, uint32_t nid);
Patches get_xml_patches(const fs::path &patch_dir, const std::string &titleid, const std::string &bin, const std::string &app_ver, uint32_t nid);
