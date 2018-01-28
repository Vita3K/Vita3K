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

#include <SceNpDrm/exports.h>

EXPORT(int, _sceNpDrmPackageCheck) {
    return unimplemented("_sceNpDrmPackageCheck");
}

EXPORT(int, _sceNpDrmPackageDecrypt) {
    return unimplemented("_sceNpDrmPackageDecrypt");
}

EXPORT(int, _sceNpDrmPackageInstallFinished) {
    return unimplemented("_sceNpDrmPackageInstallFinished");
}

EXPORT(int, _sceNpDrmPackageInstallStarted) {
    return unimplemented("_sceNpDrmPackageInstallStarted");
}

EXPORT(int, _sceNpDrmPackageTransform) {
    return unimplemented("_sceNpDrmPackageTransform");
}

EXPORT(int, _sceNpDrmPackageUninstallFinished) {
    return unimplemented("_sceNpDrmPackageUninstallFinished");
}

EXPORT(int, _sceNpDrmPackageUninstallStarted) {
    return unimplemented("_sceNpDrmPackageUninstallStarted");
}

EXPORT(int, sceNpDrmPackageInstallOngoing) {
    return unimplemented("sceNpDrmPackageInstallOngoing");
}

EXPORT(int, sceNpDrmPackageIsGameExist) {
    return unimplemented("sceNpDrmPackageIsGameExist");
}

EXPORT(int, sceNpDrmPackageUninstallOngoing) {
    return unimplemented("sceNpDrmPackageUninstallOngoing");
}

BRIDGE_IMPL(_sceNpDrmPackageCheck)
BRIDGE_IMPL(_sceNpDrmPackageDecrypt)
BRIDGE_IMPL(_sceNpDrmPackageInstallFinished)
BRIDGE_IMPL(_sceNpDrmPackageInstallStarted)
BRIDGE_IMPL(_sceNpDrmPackageTransform)
BRIDGE_IMPL(_sceNpDrmPackageUninstallFinished)
BRIDGE_IMPL(_sceNpDrmPackageUninstallStarted)
BRIDGE_IMPL(sceNpDrmPackageInstallOngoing)
BRIDGE_IMPL(sceNpDrmPackageIsGameExist)
BRIDGE_IMPL(sceNpDrmPackageUninstallOngoing)
