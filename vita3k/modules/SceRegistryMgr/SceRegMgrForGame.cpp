// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include "SceRegMgrForGame.h"

#include <regmgr/functions.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceRegMgrForGame);

EXPORT(int, sceRegMgrSystemIsBlueScreen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrSystemParamGetBin, const int id, void *buf, SceSize bufSize) {
    TRACY_FUNC(sceRegMgrSystemParamGetBin, id, buf, bufSize);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    memcpy(buf, regmgr::get_str_value(emuenv.regmgr, category, name).c_str(), bufSize);

    return 0;
}

EXPORT(int, sceRegMgrSystemParamGetInt, const int id, SceInt32 *buf) {
    TRACY_FUNC(sceRegMgrSystemParamGetInt, id, buf);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    *buf = regmgr::get_int_value(emuenv.regmgr, category, name);

    return 0;
}

EXPORT(int, sceRegMgrSystemParamGetStr, const int id, char *buf, const SceSize bufSize) {
    TRACY_FUNC(sceRegMgrSystemParamGetStr, id, buf, bufSize);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    strncpy(buf, regmgr::get_str_value(emuenv.regmgr, category, name).c_str(), bufSize);

    return 0;
}

EXPORT(int, sceRegMgrSystemParamSetBin, const int id, const void *buf, const SceSize bufSize) {
    TRACY_FUNC(sceRegMgrSystemParamSetBin, id, buf, bufSize);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    regmgr::set_bin_value(emuenv.regmgr, emuenv.pref_path, category, name, buf, bufSize);

    return 0;
}

EXPORT(int, sceRegMgrSystemParamSetInt, const int id, const SceInt32 buf) {
    TRACY_FUNC(sceRegMgrSystemParamSetInt, id, buf);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    regmgr::set_int_value(emuenv.regmgr, emuenv.pref_path, category, name, buf);

    return 0;
}

EXPORT(int, sceRegMgrSystemParamSetStr, const int id, const char *buf, const SceSize bufSize) {
    TRACY_FUNC(sceRegMgrSystemParamSetStr, id, buf, bufSize);
    const auto [category, name] = regmgr::get_category_and_name_by_id(id, export_name);
    regmgr::set_str_value(emuenv.regmgr, emuenv.pref_path, category, name, buf);

    return 0;
}

BRIDGE_IMPL(sceRegMgrSystemIsBlueScreen)
BRIDGE_IMPL(sceRegMgrSystemParamGetBin)
BRIDGE_IMPL(sceRegMgrSystemParamGetInt)
BRIDGE_IMPL(sceRegMgrSystemParamGetStr)
BRIDGE_IMPL(sceRegMgrSystemParamSetBin)
BRIDGE_IMPL(sceRegMgrSystemParamSetInt)
BRIDGE_IMPL(sceRegMgrSystemParamSetStr)
