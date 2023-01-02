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

EXPORT(int, sceRegMgrGetKeyBin) {
    TRACY_FUNC(sceRegMgrGetKeyBin);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrGetKeyInt, const char *category, const char *name, int *buf) {
    TRACY_FUNC(sceRegMgrGetKeyInt, category, name, buf);
    if ((std::string(category) == "/CONFIG/PSPEMU") && (std::string(name) == "emu_list_flag")) {
        STUBBED("Stubbed");
        *buf = 1;

        return 0;
    } else
        return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrGetKeyStr) {
    TRACY_FUNC(sceRegMgrGetKeyStr);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrGetKeys) {
    TRACY_FUNC(sceRegMgrGetKeys);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrGetKeysInfo) {
    TRACY_FUNC(sceRegMgrGetKeysInfo);
    return UNIMPLEMENTED();
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

EXPORT(int, sceRegMgrSetKeyBin) {
    TRACY_FUNC(sceRegMgrSetKeyBin);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrSetKeyInt, const char *category, const char *name, int buf) {
    TRACY_FUNC(sceRegMgrSetKeyInt, category, name, buf);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrSetKeyStr) {
    TRACY_FUNC(sceRegMgrSetKeyStr);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRegMgrSetKeys) {
    TRACY_FUNC(sceRegMgrSetKeys);
    return UNIMPLEMENTED();
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
