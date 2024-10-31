// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <module/module.h>

EXPORT(int, sceSblUsAllocateBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsCheckSystemIntegrity) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsExtractSpackage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsGetApplicableVersion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsGetExtractSpackage) {
    return UNIMPLEMENTED();
}

struct SceSblUsSpkgInfo { // Size is 0x10 on FW 0.931-0.990
    SceSize size; // size of this structure
    int version;
    int reserved1;
    int reserved2;
};

EXPORT(int, sceSblUsGetSpkgInfo, int package_type, SceSblUsSpkgInfo *pInfo) {
    LOG_DEBUG("package type: {}", package_type);
    pInfo->version = 0x3000011;

    return STUBBED("Set to 3.00");
}

EXPORT(int, sceSblUsGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsGetUpdateMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsInformUpdateFinished) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsInformUpdateOngoing) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsInformUpdateStarted) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsInspectSpackage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsPowerControl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsReleaseBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsSetSwInfoBin) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsSetSwInfoInt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsSetSwInfoStr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsSetUpdateMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsUpdateSpackage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsVerifyPup) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsVerifyPupAdditionalSign) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsVerifyPupHeader) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsVerifyPupSegment) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsVerifyPupSegmentById) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblUsVerifyPupWatermark) {
    return UNIMPLEMENTED();
}
