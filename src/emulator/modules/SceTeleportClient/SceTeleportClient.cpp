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

#include "SceTeleportClient.h"

EXPORT(int, sceTeleportClientClearCacheInfo) {
    return unimplemented("sceTeleportClientClearCacheInfo");
}

EXPORT(int, sceTeleportClientDeleteRemoteApp) {
    return unimplemented("sceTeleportClientDeleteRemoteApp");
}

EXPORT(int, sceTeleportClientEndSearchDevice) {
    return unimplemented("sceTeleportClientEndSearchDevice");
}

EXPORT(int, sceTeleportClientFinalize) {
    return unimplemented("sceTeleportClientFinalize");
}

EXPORT(int, sceTeleportClientGetCacheInfo) {
    return unimplemented("sceTeleportClientGetCacheInfo");
}

EXPORT(int, sceTeleportClientGetDeviceDescription) {
    return unimplemented("sceTeleportClientGetDeviceDescription");
}

EXPORT(int, sceTeleportClientGetRemoteAppInfo) {
    return unimplemented("sceTeleportClientGetRemoteAppInfo");
}

EXPORT(int, sceTeleportClientGetRemoteAppInfoNum) {
    return unimplemented("sceTeleportClientGetRemoteAppInfoNum");
}

EXPORT(int, sceTeleportClientInitialize) {
    return unimplemented("sceTeleportClientInitialize");
}

EXPORT(int, sceTeleportClientLaunchRemoteApp) {
    return unimplemented("sceTeleportClientLaunchRemoteApp");
}

EXPORT(int, sceTeleportClientRegisterGetDeviceInfoCallback) {
    return unimplemented("sceTeleportClientRegisterGetDeviceInfoCallback");
}

EXPORT(int, sceTeleportClientStartSearchDevice) {
    return unimplemented("sceTeleportClientStartSearchDevice");
}

EXPORT(int, sceTeleportClientWakeupLatestDevice) {
    return unimplemented("sceTeleportClientWakeupLatestDevice");
}

BRIDGE_IMPL(sceTeleportClientClearCacheInfo)
BRIDGE_IMPL(sceTeleportClientDeleteRemoteApp)
BRIDGE_IMPL(sceTeleportClientEndSearchDevice)
BRIDGE_IMPL(sceTeleportClientFinalize)
BRIDGE_IMPL(sceTeleportClientGetCacheInfo)
BRIDGE_IMPL(sceTeleportClientGetDeviceDescription)
BRIDGE_IMPL(sceTeleportClientGetRemoteAppInfo)
BRIDGE_IMPL(sceTeleportClientGetRemoteAppInfoNum)
BRIDGE_IMPL(sceTeleportClientInitialize)
BRIDGE_IMPL(sceTeleportClientLaunchRemoteApp)
BRIDGE_IMPL(sceTeleportClientRegisterGetDeviceInfoCallback)
BRIDGE_IMPL(sceTeleportClientStartSearchDevice)
BRIDGE_IMPL(sceTeleportClientWakeupLatestDevice)
