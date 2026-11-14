// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

struct sceFiosKernelOverlayResolveWithRangeSync02_opt {
    Ptr<char> out_path;
    SceSize max_path;
    SceUInt8 min_order;
    SceUInt8 max_order;
    SceUInt8 reserved0[2];
    SceUInt32 reserved1;
    SceSize out_path_buffer_size;
    SceUInt32 reserved2[2];
};
static_assert(sizeof(sceFiosKernelOverlayResolveWithRangeSync02_opt) == 0x1C);

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
    TRACY_FUNC(sceFiosKernelOverlayResolveWithRangeSync02, pid, resolveFlag, pInPath, opt);
    STUBBED("Using strncpy");
    strncpy(opt->out_path.get(emuenv.mem), pInPath, opt->max_path);

    return 0;
}

EXPORT(int, sceFiosKernelOverlayThreadIsDisabled02) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiosKernelOverlayThreadSetDisabled02) {
    return UNIMPLEMENTED();
}
