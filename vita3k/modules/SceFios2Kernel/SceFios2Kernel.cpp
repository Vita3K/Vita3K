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

#include <util/tracy.h>
TRACY_MODULE_NAME(SceFios2Kernel);

struct sceFiosKernelOverlayResolveWithRangeSync_opt {
    Ptr<char> pOutPath;
    SceSize maxPath;
    char loOrderFilter;
    char hiOrderFilter;
    char reserved1;
    char reserved2;
    int reserved3;
    int reserved4;
    int reserved5;
    int reserved6;
};

EXPORT(int, _sceFiosKernelOverlayAdd) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayAddForProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayDHChstatSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayDHCloseSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayDHOpenSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayDHReadSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayDHStatSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayDHSyncSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayGetInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayGetInfoForProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayGetList) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayGetRecommendedScheduler) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayModify) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayModifyForProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayRemove) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayRemoveForProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayResolveSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayResolveWithRangeSync, SceUID pid, int resolveFlag, const char *pInPath, sceFiosKernelOverlayResolveWithRangeSync_opt *opt) {
    TRACY_FUNC(_sceFiosKernelOverlayResolveWithRangeSync, pid, resolveFlag, pInPath, opt);
    STUBBED("Using strncpy");
    strncpy(opt->pOutPath.get(emuenv.mem), pInPath, opt->maxPath);

    return 0;
}

EXPORT(int, _sceFiosKernelOverlayThreadIsDisabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayThreadSetDisabled) {
    return UNIMPLEMENTED();
}
