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

#include "SceFios2Kernel02.h"

EXPORT(int, sceFiosKernelOverlayAddForProcess02) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiosKernelOverlayGetInfoForProcess02) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiosKernelOverlayGetList02) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiosKernelOverlayGetRecommendedScheduler02) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiosKernelOverlayModifyForProcess02) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiosKernelOverlayRemoveForProcess02) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiosKernelOverlayResolveSync02) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiosKernelOverlayResolveWithRangeSync02, SceUID pid, int resolveFlag, const char *pInPath, sceFiosKernelOverlayResolveWithRangeSync02_opt *opt) {
    STUBBED("Using strncpy");
    strncpy(opt->pOutPath.get(host.mem), pInPath, opt->maxPath);

    return 0;
}

EXPORT(int, sceFiosKernelOverlayThreadIsDisabled02) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiosKernelOverlayThreadSetDisabled02) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceFiosKernelOverlayAddForProcess02)
BRIDGE_IMPL(sceFiosKernelOverlayGetInfoForProcess02)
BRIDGE_IMPL(sceFiosKernelOverlayGetList02)
BRIDGE_IMPL(sceFiosKernelOverlayGetRecommendedScheduler02)
BRIDGE_IMPL(sceFiosKernelOverlayModifyForProcess02)
BRIDGE_IMPL(sceFiosKernelOverlayRemoveForProcess02)
BRIDGE_IMPL(sceFiosKernelOverlayResolveSync02)
BRIDGE_IMPL(sceFiosKernelOverlayResolveWithRangeSync02)
BRIDGE_IMPL(sceFiosKernelOverlayThreadIsDisabled02)
BRIDGE_IMPL(sceFiosKernelOverlayThreadSetDisabled02)
