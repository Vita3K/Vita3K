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

#include "patch/patch.h"
#include "patch/util.h"

#include <util/log.h>
#include <util/string_utils.h>

#include <pugixml.hpp>

#include <algorithm>

namespace {

std::string file_name_of(const std::string &bin) {
    const auto pos = bin.find_last_of("/\\:");
    return pos == std::string::npos ? bin : bin.substr(pos + 1);
}

} // namespace

std::string PatchEntry::key() const {
    return (title_ids.empty() ? std::string{} : title_ids.front()) + '|' + app_ver + '|' + app_elf + '|' + nid + '|' + name;
}

fs::path patchlist_xml_path(const fs::path &patch_dir) {
    return patch_dir / "patchlist.xml";
}

fs::path patch_config_path(const fs::path &patch_dir) {
    return patch_dir / "patch_config.xml";
}

bool parse_patch_entries(const std::string &xml, PatchEntries &entries) {
    pugi::xml_document doc;
    if (!doc.load_buffer(xml.data(), xml.size()))
        return false;

    const auto root = doc.child("Patch");
    if (!root)
        return false;

    std::vector<std::string> title_ids;
    for (const auto node : root.children()) {
        const std::string_view tag = node.name();
        if (tag == "TitleID") {
            title_ids.clear();
            for (const auto id : node.children("ID"))
                title_ids.emplace_back(id.text().as_string());
            continue;
        }
        if (tag != "Metadata")
            continue;

        PatchEntry entry;
        entry.title_ids = title_ids;
        entry.title = node.attribute("Title").as_string();
        entry.name = node.attribute("Name").as_string();
        entry.author = node.attribute("Author").as_string();
        entry.note = node.attribute("Note").as_string();
        entry.patch_ver = node.attribute("PatchVer").as_string();
        entry.app_ver = node.attribute("AppVer").as_string();
        entry.app_elf = node.attribute("AppElf").as_string("eboot.bin");
        entry.nid = node.attribute("NID").as_string();

        for (const auto line : node.child("PatchList").children("Line")) {
            const std::string type = line.attribute("Type").as_string();
            Patch patch{};
            const auto values = parse_value(line.attribute("Value").as_string());
            if (type != "bytes" || !values || !parse_address(line.attribute("Address").as_string(), patch.seg, patch.offset)) {
                LOG_WARN("Skipping unsupported patch line in {} ({}): type={} address={}", entry.title, entry.name, type, line.attribute("Address").as_string());
                continue;
            }
            patch.values = *values;
            entry.patches.push_back(std::move(patch));
        }
        entries.push_back(std::move(entry));
    }

    return true;
}

PatchEntries load_patch_entries(const fs::path &patch_dir) {
    std::vector<char> buffer;
    PatchEntries entries;
    const auto path = patchlist_xml_path(patch_dir);
    if (!fs_utils::read_data(path, buffer))
        return entries;

    if (!parse_patch_entries(std::string(buffer.begin(), buffer.end()), entries))
        LOG_ERROR("Failed to parse {}", fs_utils::path_to_utf8(path));
    return entries;
}

EnabledPatches load_enabled_patches(const fs::path &patch_dir) {
    EnabledPatches enabled;
    pugi::xml_document doc;
    if (!doc.load_file(patch_config_path(patch_dir).c_str()))
        return enabled;

    for (const auto node : doc.child("PatchConfig").children("Enabled"))
        enabled.insert(node.attribute("Key").as_string());
    return enabled;
}

bool save_enabled_patches(const fs::path &patch_dir, const EnabledPatches &enabled) {
    pugi::xml_document doc;
    auto root = doc.append_child("PatchConfig");
    for (const auto &key : enabled)
        root.append_child("Enabled").append_attribute("Key").set_value(key.c_str());
    return doc.save_file(patch_config_path(patch_dir).c_str());
}

bool patch_entry_matches(const PatchEntry &entry, const std::string &titleid, const std::string &bin, const std::string &app_ver, uint32_t nid) {
    if (std::find(entry.title_ids.begin(), entry.title_ids.end(), titleid) == entry.title_ids.end())
        return false;

    if (string_utils::tolower(file_name_of(bin)) != string_utils::tolower(file_name_of(entry.app_elf)))
        return false;

    if (!entry.app_ver.empty() && !patch_versions_equal(entry.app_ver, app_ver))
        return false;

    const auto expected = parse_hex(entry.nid);
    return expected && *expected == nid;
}

Patches get_xml_patches(const fs::path &patch_dir, const std::string &titleid, const std::string &bin, const std::string &app_ver, uint32_t nid) {
    Patches patches;
    const auto enabled = load_enabled_patches(patch_dir);
    if (enabled.empty())
        return patches;

    for (const auto &entry : load_patch_entries(patch_dir)) {
        if (!enabled.contains(entry.key()) || !patch_entry_matches(entry, titleid, bin, app_ver, nid))
            continue;

        LOG_INFO("Enabled patch: {} ({} {})", entry.title, entry.name, entry.patch_ver);
        patches.insert(patches.end(), entry.patches.begin(), entry.patches.end());
    }

    return patches;
}
