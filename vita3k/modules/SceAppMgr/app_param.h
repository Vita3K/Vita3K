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

#include <packages/sfo.h>

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

struct AppMgrShellStringParam {
    std::string_view value;
    size_t minimum_size;
};

inline std::optional<std::string> appmgr_savedata_id(const SfoFile &sfo) {
    const SfoFile::SfoEntry *savedata = nullptr;
    const SfoFile::SfoEntry *title = nullptr;
    for (const auto &entry : sfo.entries) {
        auto **field = entry.data.first == "INSTALL_DIR_SAVEDATA" ? &savedata
            : entry.data.first == "TITLE_ID"                      ? &title
                                                                  : nullptr;
        if (field) {
            if (*field)
                return std::nullopt;
            *field = &entry;
        }
    }
    const auto *selected = savedata ? savedata : title;
    if (!selected || selected->entry.data_fmt != SfoDataFormat::UTF8_NULL)
        return std::nullopt;
    const auto &value = selected->data.second;
    if (value.empty() || value == "." || value == ".."
        || value.find_first_of("/\\:") != std::string::npos || value.find('\0') != std::string::npos)
        return std::nullopt;
    return value;
}

inline std::optional<AppMgrShellStringParam> appmgr_shell_string_param(int param) {
    switch (param) {
    case 6: return AppMgrShellStringParam{ "", 48 };
    case 7: return AppMgrShellStringParam{ "", 13 };
    case 8: return AppMgrShellStringParam{ "", 4 };
    case 9: return AppMgrShellStringParam{ "", 128 };
    case 10: return AppMgrShellStringParam{ "", 52 };
    case 12: return AppMgrShellStringParam{ "NPXS19999", 10 };
    case 14: return AppMgrShellStringParam{ "", 292 };
    default: return std::nullopt;
    }
}

inline std::optional<uint32_t> appmgr_sfo_attribute(const SfoFile &sfo, int param) {
    const std::string_view key = param == 15 ? "ATTRIBUTE" : param == 17 ? "ATTRIBUTE2"
                                                                         : "";
    if (key.empty())
        return std::nullopt;
    const auto entry = std::ranges::find_if(sfo.entries, [&](const auto &item) { return item.data.first == key; });
    if (entry == sfo.entries.end())
        return 0;
    if (entry->entry.data_fmt != SfoDataFormat::UINT32_T || entry->entry.data_len != 4 || entry->entry.data_max_len != 4)
        return std::nullopt;
    uint32_t value{};
    const auto &data = entry->data.second;
    const auto result = std::from_chars(data.data(), data.data() + data.size(), value);
    if (result.ec != std::errc{} || result.ptr != data.data() + data.size())
        return std::nullopt;
    return value;
}

inline std::optional<std::string> appmgr_sfo_category(const SfoFile &sfo) {
    const auto entry = std::ranges::find_if(sfo.entries, [](const auto &item) { return item.data.first == "CATEGORY"; });
    if (entry == sfo.entries.end() || entry->entry.data_fmt != SfoDataFormat::UTF8_NULL
        || entry->entry.data_len <= 2 || entry->entry.data_max_len != 4
        || entry->data.second.size() < 2 || entry->data.second.size() > 3)
        return std::nullopt;
    return entry->data.second;
}