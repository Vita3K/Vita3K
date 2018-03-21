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

#include <io/vfs.h>
#include <util/log.h>

#include <map>

namespace vfs {
    std::map<std::string, std::string> mount_map;
    std::mutex mut;

    bool escape(const char c) {
        return (c == '/' || c == '\\');
    }

    const char escape() {
#ifdef WIN32
        return '\\';
#else
        return '/';
#endif
    }

    const char escape_vita() {
        return '/';
    }

    bool mount(const std::string& dvc, const std::string& physical_path) {
         auto res = mount_map.find(dvc);

         if (res != mount_map.end()) {
             LOG_WARN("Trying to mount already-mounted {}", dvc);
         }

         std::lock_guard<std::mutex> guard(mut);

         return mount_map.emplace(dvc, physical_path).second;
    }

    bool unmount(const std::string& dvc) {
        std::lock_guard<std::mutex> guard(mut);

        auto res = mount_map.erase(dvc);
        return true;
    }

    std::string physical_mount_path(const std::string& dvc) {
        auto res = mount_map.find(dvc);

        if (res==mount_map.end()) {
            LOG_INFO("Partition {} physical path not found, use default", dvc);
            return "";
        }

        return res->second;
    }

    std::string get(const std::string& dvc_path) {
        const int index = dvc_path.find_first_of(":");

        if (index == std::string::npos) {
            LOG_ERROR("Fail trying to get the PSVITA path: {}", dvc_path);
            return "";
        }

        std::string dvc  = dvc_path.substr(0, index);
        std::string pref_path = vfs::physical_mount_path(dvc);

        uint32_t finalIndex = index+1;

        while (escape(dvc_path[finalIndex])) {
            finalIndex++;
        };

        std::string resStr = pref_path + dvc_path.substr(finalIndex);

        return resStr;
    }
}
