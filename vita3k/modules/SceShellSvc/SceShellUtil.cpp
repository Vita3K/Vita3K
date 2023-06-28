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

#include "SceShellUtil.h"

enum SceShellUtilLockType {
    SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN = 0x1,
    SCE_SHELL_UTIL_LOCK_TYPE_QUICK_MENU = 0x2,
    SCE_SHELL_UTIL_LOCK_TYPE_POWEROFF_MENU = 0x4,
    SCE_SHELL_UTIL_LOCK_TYPE_UNK8 = 0x8,
    SCE_SHELL_UTIL_LOCK_TYPE_USB_CONNECTION = 0x10,
    SCE_SHELL_UTIL_LOCK_TYPE_MC_INSERTED = 0x20,
    SCE_SHELL_UTIL_LOCK_TYPE_MC_REMOVED = 0x40,
    SCE_SHELL_UTIL_LOCK_TYPE_UNK80 = 0x80,
    SCE_SHELL_UTIL_LOCK_TYPE_UNK100 = 0x100,
    SCE_SHELL_UTIL_LOCK_TYPE_UNK200 = 0x200,
    SCE_SHELL_UTIL_LOCK_TYPE_MUSIC_PLAYER = 0x400,
    SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2 = 0x800 //! without the stop symbol
};

struct SceShellUtilLaunchAppParam {
    const char *cmd;
};

EXPORT(int, sceShellUtilInitEvents) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceShellUtilLock, const SceShellUtilLockType type) {
    LOG_DEBUG("type: {}", log_hex(type));
    return UNIMPLEMENTED();
}

EXPORT(int, sceShellUtilRegisterEventHandler) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceShellUtilRequestLaunchApp, SceShellUtilLaunchAppParam *param) {
    LOG_DEBUG("cmd: {}", param->cmd);
    return UNIMPLEMENTED();
}

EXPORT(int, sceShellUtilUnlock, const SceShellUtilLockType type) {
    LOG_DEBUG("type: {}", log_hex(type));
    return UNIMPLEMENTED();
}

EXPORT(int, SceShellUtil_EC5881A5) {
    return UNIMPLEMENTED();
}

EXPORT(int, SceShellUtil_CE35B2B8) {
    return UNIMPLEMENTED();
}

EXPORT(int, SceShellUtil_9B0EE918) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceShellUtilInitEvents)
BRIDGE_IMPL(sceShellUtilLock)
BRIDGE_IMPL(sceShellUtilRegisterEventHandler)
BRIDGE_IMPL(sceShellUtilRequestLaunchApp)
BRIDGE_IMPL(sceShellUtilUnlock)
BRIDGE_IMPL(SceShellUtil_EC5881A5)
BRIDGE_IMPL(SceShellUtil_CE35B2B8)
BRIDGE_IMPL(SceShellUtil_9B0EE918)
