// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <pfsmgr/mounts.h>
#include <util/log.h>

#include <algorithm>
#include <cstdio>
#include <random>
#include <unordered_set>

namespace pfsmgr {

std::string allocate_mount_name(std::string_view prefix) {
    static std::mutex mutex;
    static std::unordered_set<std::string> allocated;
    std::lock_guard lock(mutex);
    try {
        std::random_device random;
        std::uniform_int_distribution<uint64_t> distribution(1, (prefix == "gp" || prefix == "tm") ? 0xfffffffffffULL : 0xffffffffffffULL);
        for (int attempt = 0; attempt < 64; ++attempt) {
            const auto id = static_cast<unsigned long long>(distribution(random));
            char name[20];
            if (prefix == "gp" || prefix == "tm")
                std::snprintf(name, sizeof(name), "%.*s%011llxd:", static_cast<int>(prefix.size()), prefix.data(), id);
            else if (prefix == "ad")
                std::snprintf(name, sizeof(name), "ad%012llx:", id);
            else
                std::snprintf(name, sizeof(name), "PD%012llxd0:", id);
            if (allocated.emplace(name).second)
                return name;
        }
    } catch (const std::exception &error) {
        LOG_ERROR("Cannot allocate mount name: {}", error.what());
        return {};
    }
    LOG_ERROR("Cannot allocate unique mount name for '{}'", prefix);
    return {};
}

bool is_mount_name(const std::string &name, std::string_view prefix) {
    const bool pfs = prefix == "PD";
    if (name.size() != (pfs ? 17 : 15) || !name.starts_with(prefix) || name.back() != ':')
        return false;
    if (((prefix == "gp" || prefix == "tm") && name[13] != 'd') || (pfs && (name[14] != 'd' || name[15] != '0')))
        return false;
    const auto hex_end = name.end() - (pfs ? 3 : (prefix == "gp" || prefix == "tm") ? 2
                                                                                    : 1);
    return std::all_of(name.begin() + 2, hex_end, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

} // namespace pfsmgr