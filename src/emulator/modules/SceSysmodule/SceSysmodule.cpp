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

#include "SceSysmodule.h"

EXPORT(int, sceSysmoduleIsLoaded) {
    return unimplemented("sceSysmoduleIsLoaded");
}

EXPORT(int, sceSysmoduleIsLoadedInternal) {
    return unimplemented("sceSysmoduleIsLoadedInternal");
}

EXPORT(int, sceSysmoduleLoadModule) {
    return unimplemented("sceSysmoduleLoadModule");
}

EXPORT(int, sceSysmoduleLoadModuleInternal) {
    return unimplemented("sceSysmoduleLoadModuleInternal");
}

EXPORT(int, sceSysmoduleLoadModuleInternalWithArg) {
    return unimplemented("sceSysmoduleLoadModuleInternalWithArg");
}

EXPORT(int, sceSysmoduleUnloadModule) {
    return unimplemented("sceSysmoduleUnloadModule");
}

EXPORT(int, sceSysmoduleUnloadModuleInternal) {
    return unimplemented("sceSysmoduleUnloadModuleInternal");
}

EXPORT(int, sceSysmoduleUnloadModuleInternalWithArg) {
    return unimplemented("sceSysmoduleUnloadModuleInternalWithArg");
}

BRIDGE_IMPL(sceSysmoduleIsLoaded)
BRIDGE_IMPL(sceSysmoduleIsLoadedInternal)
BRIDGE_IMPL(sceSysmoduleLoadModule)
BRIDGE_IMPL(sceSysmoduleLoadModuleInternal)
BRIDGE_IMPL(sceSysmoduleLoadModuleInternalWithArg)
BRIDGE_IMPL(sceSysmoduleUnloadModule)
BRIDGE_IMPL(sceSysmoduleUnloadModuleInternal)
BRIDGE_IMPL(sceSysmoduleUnloadModuleInternalWithArg)
