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

#include <module/module.h>

#include <emuenv/app_util.h>
#include <emuenv/state.h>
#include <io/state.h>

EXPORT(int, sceAppUtilCacheGetDevInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilCacheMount) {
    const auto cache_path = "cache/" + emuenv.io.title_id;
    boost::system::error_code ec;
    fs::create_directories(emuenv.vita_fs_path / "ux0" / cache_path, ec);
    if (ec) {
        LOG_ERROR("Failed to create cache directory: {}", ec.message());
        return RET_ERROR(SCE_APPUTIL_ERROR_NOT_MOUNTED);
    }

    emuenv.io.device_paths.cache0 = cache_path;
    return 0;
}

EXPORT(int, sceAppUtilCacheUmount) {
    if (emuenv.io.device_paths.cache0.empty())
        return RET_ERROR(SCE_APPUTIL_ERROR_NOT_MOUNTED);

    emuenv.io.device_paths.cache0.clear();
    return 0;
}
