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

#include "SceNpDrmPackage.h"

EXPORT(int, _sceNpDrmPackageCheck) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceNpDrmPackageDecrypt) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceNpDrmPackageInstallFinished) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceNpDrmPackageInstallStarted) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceNpDrmPackageTransform) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceNpDrmPackageUninstallFinished) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceNpDrmPackageUninstallStarted) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceNpDrmSaveDataFormatFinished) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceNpDrmSaveDataFormatStarted) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceNpDrmSaveDataInstallFinished) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceNpDrmSaveDataInstallStarted) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpDrmPackageInstallOngoing) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpDrmPackageIsGameExist) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpDrmPackageUninstallOngoing) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpDrmSaveDataFormatOngoing) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpDrmSaveDataInstallOngoing) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceNpDrmPackageCheck)
BRIDGE_IMPL(_sceNpDrmPackageDecrypt)
BRIDGE_IMPL(_sceNpDrmPackageInstallFinished)
BRIDGE_IMPL(_sceNpDrmPackageInstallStarted)
BRIDGE_IMPL(_sceNpDrmPackageTransform)
BRIDGE_IMPL(_sceNpDrmPackageUninstallFinished)
BRIDGE_IMPL(_sceNpDrmPackageUninstallStarted)
BRIDGE_IMPL(_sceNpDrmSaveDataFormatFinished)
BRIDGE_IMPL(_sceNpDrmSaveDataFormatStarted)
BRIDGE_IMPL(_sceNpDrmSaveDataInstallFinished)
BRIDGE_IMPL(_sceNpDrmSaveDataInstallStarted)
BRIDGE_IMPL(sceNpDrmPackageInstallOngoing)
BRIDGE_IMPL(sceNpDrmPackageIsGameExist)
BRIDGE_IMPL(sceNpDrmPackageUninstallOngoing)
BRIDGE_IMPL(sceNpDrmSaveDataFormatOngoing)
BRIDGE_IMPL(sceNpDrmSaveDataInstallOngoing)
