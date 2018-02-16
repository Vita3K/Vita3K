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

#include "SceNpDrm.h"

EXPORT(int, _sceNpDrmCheckActData) {
    return unimplemented("_sceNpDrmCheckActData");
}

EXPORT(int, _sceNpDrmCheckDrmReset) {
    return unimplemented("_sceNpDrmCheckDrmReset");
}

EXPORT(int, _sceNpDrmGetFixedRifName) {
    return unimplemented("_sceNpDrmGetFixedRifName");
}

EXPORT(int, _sceNpDrmGetRifInfo) {
    return unimplemented("_sceNpDrmGetRifInfo");
}

EXPORT(int, _sceNpDrmGetRifName) {
    return unimplemented("_sceNpDrmGetRifName");
}

EXPORT(int, _sceNpDrmGetRifNameForInstall) {
    return unimplemented("_sceNpDrmGetRifNameForInstall");
}

EXPORT(int, _sceNpDrmPresetRifProvisionalFlag) {
    return unimplemented("_sceNpDrmPresetRifProvisionalFlag");
}

EXPORT(int, _sceNpDrmRemoveActData) {
    return unimplemented("_sceNpDrmRemoveActData");
}

BRIDGE_IMPL(_sceNpDrmCheckActData)
BRIDGE_IMPL(_sceNpDrmCheckDrmReset)
BRIDGE_IMPL(_sceNpDrmGetFixedRifName)
BRIDGE_IMPL(_sceNpDrmGetRifInfo)
BRIDGE_IMPL(_sceNpDrmGetRifName)
BRIDGE_IMPL(_sceNpDrmGetRifNameForInstall)
BRIDGE_IMPL(_sceNpDrmPresetRifProvisionalFlag)
BRIDGE_IMPL(_sceNpDrmRemoveActData)
