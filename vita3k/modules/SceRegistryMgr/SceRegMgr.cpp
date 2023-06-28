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

#include "SceRegMgr.h"

#include <regmgr/functions.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceRegMgr);

EXPORT(int, sceRegMgrAddRegistryCallback) {
    TRACY_FUNC(sceRegMgrAddRegistryCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrDbBackup) {
    TRACY_FUNC(sceRegMgrDbBackup);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrDbRestore) {
    TRACY_FUNC(sceRegMgrDbRestore);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrGetInitVals) {
    TRACY_FUNC(sceRegMgrGetInitVals);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrGetKeyBin, const char *category, const char *name, void *buf, SceSize bufSize) {
    TRACY_FUNC(sceRegMgrGetKeyBin, category, name, buf, bufSize);
    regmgr::get_bin_value(emuenv.regmgr, category, name, buf, bufSize);

    return 0;
}

EXPORT(int, sceRegMgrGetKeyInt, const char *category, const char *name, SceInt32 *buf) {
    TRACY_FUNC(sceRegMgrGetKeyInt, category, name, buf);
    *buf = regmgr::get_int_value(emuenv.regmgr, category, name);

    return 0;
}

EXPORT(int, sceRegMgrGetKeyStr, const char *category, const char *name, char *buf, const SceSize bufSize) {
    TRACY_FUNC(sceRegMgrGetKeyStr, category, name, buf, bufSize);
    strncpy(buf, regmgr::get_str_value(emuenv.regmgr, category, name).c_str(), bufSize);

    return 0;
}

EXPORT(int, sceRegMgrGetKeys, const char *category, Keys *keys, const SceUInt32 elementsNumber) {
    TRACY_FUNC(sceRegMgrGetKeys, category, keys, elementsNumber);
    for (SceUInt32 i = 0; i < elementsNumber; i++) {
        keys[i].value = regmgr::get_int_value(emuenv.regmgr, category, keys[i].name.get(emuenv.mem));
    }

    return 0;
}

EXPORT(int, sceRegMgrGetKeysInfo, const char *category, Keys *keys, const SceInt32 elementsNumber) {
    TRACY_FUNC(sceRegMgrGetKeysInfo, category, keys, elementsNumber);
    LOG_DEBUG("size: {}/{}", keys->size, sizeof(Keys));
    for (auto i = 0; i < elementsNumber; i++) {
        LOG_DEBUG("key: {}/{}", category, keys[i].name.get(emuenv.mem), sizeof(Keys));
        keys[i].type = 0;
    }

    return STUBBED("Return 0 to type");
}

EXPORT(int, sceRegMgrGetRegVersion) {
    TRACY_FUNC(sceRegMgrGetRegVersion);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrIsBlueScreen) {
    TRACY_FUNC(sceRegMgrIsBlueScreen);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrRegisterCallback) {
    TRACY_FUNC(sceRegMgrRegisterCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrRegisterDrvErrCallback) {
    TRACY_FUNC(sceRegMgrRegisterDrvErrCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrResetRegistryLv) {
    TRACY_FUNC(sceRegMgrResetRegistryLv);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrSetKeyBin, const char *category, const char *name, const void *buf, SceSize bufSize) {
    TRACY_FUNC(sceRegMgrSetKeyBin, category, name, buf, bufSize);
    regmgr::set_bin_value(emuenv.regmgr, category, name, buf, bufSize);

    return 0;
}

EXPORT(int, sceRegMgrSetKeyInt, const char *category, const char *name, const SceInt32 buf) {
    TRACY_FUNC(sceRegMgrSetKeyInt, category, name, buf);
    regmgr::set_int_value(emuenv.regmgr, category, name, buf);

    return 0;
}

EXPORT(int, sceRegMgrSetKeyStr, const char *category, const char *name, char *buf, const SceSize bufSize) {
    TRACY_FUNC(sceRegMgrSetKeyStr, category, name, buf, bufSize);
    regmgr::set_str_value(emuenv.regmgr, category, name, buf, bufSize);

    return 0;
}

EXPORT(int, sceRegMgrSetKeys, const char *category, const Keys *keys, const SceInt32 elementsNumber) {
    TRACY_FUNC(sceRegMgrSetKeys, category, keys, elementsNumber);
    for (auto i = 0; i < elementsNumber; i++) {
        regmgr::set_int_value(emuenv.regmgr, category, keys[i].name.get(emuenv.mem), keys[i].value);
    }

    return 0;
}

EXPORT(int, sceRegMgrStartCallback) {
    TRACY_FUNC(sceRegMgrStartCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrStopCallback) {
    TRACY_FUNC(sceRegMgrStopCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrUnregisterCallback) {
    TRACY_FUNC(sceRegMgrUnregisterCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrUnregisterDrvErrCallback) {
    TRACY_FUNC(sceRegMgrUnregisterDrvErrCallback);
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceRegMgrAddRegistryCallback)
BRIDGE_IMPL(sceRegMgrDbBackup)
BRIDGE_IMPL(sceRegMgrDbRestore)
BRIDGE_IMPL(sceRegMgrGetInitVals)
BRIDGE_IMPL(sceRegMgrGetKeyBin)
BRIDGE_IMPL(sceRegMgrGetKeyInt)
BRIDGE_IMPL(sceRegMgrGetKeyStr)
BRIDGE_IMPL(sceRegMgrGetKeys)
BRIDGE_IMPL(sceRegMgrGetKeysInfo)
BRIDGE_IMPL(sceRegMgrGetRegVersion)
BRIDGE_IMPL(sceRegMgrIsBlueScreen)
BRIDGE_IMPL(sceRegMgrRegisterCallback)
BRIDGE_IMPL(sceRegMgrRegisterDrvErrCallback)
BRIDGE_IMPL(sceRegMgrResetRegistryLv)
BRIDGE_IMPL(sceRegMgrSetKeyBin)
BRIDGE_IMPL(sceRegMgrSetKeyInt)
BRIDGE_IMPL(sceRegMgrSetKeyStr)
BRIDGE_IMPL(sceRegMgrSetKeys)
BRIDGE_IMPL(sceRegMgrStartCallback)
BRIDGE_IMPL(sceRegMgrStopCallback)
BRIDGE_IMPL(sceRegMgrUnregisterCallback)
BRIDGE_IMPL(sceRegMgrUnregisterDrvErrCallback)
