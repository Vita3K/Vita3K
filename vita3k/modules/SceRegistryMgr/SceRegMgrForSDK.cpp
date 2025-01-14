// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <regmgr/functions.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceRegMgrForSdk);

EXPORT(int, sceRegMgrUtilityGetBin, const int id, void *buf, const SceSize bufSize) {
    TRACY_FUNC(sceRegMgrUtilityGetBin, id, buf, bufSize);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    regmgr::get_bin_value(emuenv.regmgr, category, name, buf, bufSize);

    return 0;
}

EXPORT(int, sceRegMgrUtilityGetInt, const int id, SceInt32 *buf) {
    TRACY_FUNC(sceRegMgrUtilityGetInt, id, buf);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    *buf = regmgr::get_int_value(emuenv.regmgr, category, name);

    return 0;
}

EXPORT(int, sceRegMgrUtilityGetStr, const int id, char *buf, const SceSize bufSize) {
    TRACY_FUNC(sceRegMgrUtilityGetStr, id, buf, bufSize);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    strncpy(buf, regmgr::get_str_value(emuenv.regmgr, category, name).c_str(), bufSize);

    return 0;
}

EXPORT(int, sceRegMgrUtilitySetBin, const int id, const void *buf, const SceSize bufSize) {
    TRACY_FUNC(sceRegMgrUtilitySetBin, id, buf, bufSize);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    regmgr::set_bin_value(emuenv.regmgr, category, name, buf, bufSize);

    return 0;
}

EXPORT(int, sceRegMgrUtilitySetInt, const int id, SceInt32 buf) {
    TRACY_FUNC(sceRegMgrUtilitySetInt, id, buf);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    regmgr::set_int_value(emuenv.regmgr, category, name, buf);

    return 0;
}

EXPORT(int, sceRegMgrUtilitySetStr, const int id, const char *buf, const SceSize bufSize) {
    TRACY_FUNC(sceRegMgrUtilitySetStr, id, buf, bufSize);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    regmgr::set_str_value(emuenv.regmgr, category, name, buf, bufSize);

    return 0;
}
