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

int unimplemented(const char *name) {
#ifndef VERBOSE    
    bool inserted = false;
    {
        const std::lock_guard<std::mutex> lock(mutex);
        inserted = logged.insert(name).second;
    }

    if (inserted)
#endif    
    {
        LOG_WARN(">>> {} <<< Unimplemented import called.", name);
    }

    return 0;
}

int error(const char *name, int error) {
    LOG_ERROR(">>> {} <<< returned {0:#X}", name, error);
  
    return error;
}
