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

#include "SceGps.h"

EXPORT(int, _sceGpsClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGpsGetData) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGpsGetDeviceInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGpsGetState) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGpsIoctl) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGpsIsDevice) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGpsOpen) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGpsResumeCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGpsSelectDevice) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGpsStart) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceGpsStop) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceGpsClose)
BRIDGE_IMPL(_sceGpsGetData)
BRIDGE_IMPL(_sceGpsGetDeviceInfo)
BRIDGE_IMPL(_sceGpsGetState)
BRIDGE_IMPL(_sceGpsIoctl)
BRIDGE_IMPL(_sceGpsIsDevice)
BRIDGE_IMPL(_sceGpsOpen)
BRIDGE_IMPL(_sceGpsResumeCallback)
BRIDGE_IMPL(_sceGpsSelectDevice)
BRIDGE_IMPL(_sceGpsStart)
BRIDGE_IMPL(_sceGpsStop)
