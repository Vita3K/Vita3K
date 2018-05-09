// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <module/module.h>
#include <util/log.h>

#include <iostream>
#include <mutex>
#include <set>

typedef std::set<std::string> NameSet;

static std::mutex mutex;
static NameSet logged;

int unimplemented_impl(const char *name) {
    bool inserted = false;
    {
        const std::lock_guard<std::mutex> lock(mutex);
        inserted = logged.insert(name).second;
    }

    if (inserted) {
        LOG_WARN("Unimplemented {} import called.", name);
    }

    return 0;
}

int stubbed_impl(const char *name, const char *info) {
    bool inserted = false;
    {
        const std::lock_guard<std::mutex> lock(mutex);
        inserted = logged.insert(name).second;
    }

    if (inserted) {
        LOG_WARN("Stubbed {} import called. ({})", name, info);
    }

    return 0;
}

int ret_error_impl(const char *name, const char *error_str, std::uint32_t error_val) {
    bool inserted = false;

    {
        const std::lock_guard<std::mutex> lock(mutex);
        inserted = logged.insert(name).second;
    }

    if (inserted) {
        LOG_ERROR("{} returned {} ({:#x})", name, error_str, error_val);
    }

    return error_val;
}
