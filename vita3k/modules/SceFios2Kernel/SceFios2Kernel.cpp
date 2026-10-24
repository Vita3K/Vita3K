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

#include <io/functions.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceFios2Kernel);

struct sceFiosKernelOverlayResolveWithRangeSync_opt {
    Ptr<char> pOutPath;
    SceSize maxPath;
    uint8_t loOrderFilter;
    uint8_t hiOrderFilter;
    uint8_t reserved1[2];
    SceSize inPathSize;
    SceSize outPathSize;
    SceUInt32 reserved2;
};

enum SceFiosKernelErrorCode : uint32_t {
    SCE_FIOS_ERROR_INVALID_PATH = 0x80820005,
    SCE_FIOS_ERROR_BAD_MAX_PATH = 0x80820008,
    SCE_FIOS_ERROR_IN_PATH_TOO_LONG = 0x80820018,
    SCE_FIOS_ERROR_OUT_PATH_TOO_BIG = 0x8002710B
};

static constexpr SceSize FIOS_MAX_RESOLVE_PATH = 0x400;

EXPORT(int, _sceFiosKernelOverlayAdd) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayAddForProcess, SceUID pid, SceFiosProcessOverlay *pOverlay, SceUID *pOutID) {
    TRACY_FUNC(_sceFiosKernelOverlayAddForProcess, pid, pOverlay, pOutID);
    if (pOverlay->type != SCE_FIOS_OVERLAY_TYPE_OPAQUE)
        LOG_WARN("Using unimplemented overlay type {}.", fmt::underlying(pOverlay->type));

    if (strnlen(pOverlay->dst, sizeof(pOverlay->dst)) == sizeof(pOverlay->dst) || strnlen(pOverlay->src, sizeof(pOverlay->src)) == sizeof(pOverlay->src))
        return SCE_FIOS_ERROR_INVALID_PATH;

    *pOutID = create_overlay(emuenv.io, pOverlay);

    return 0;
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
    if (opt->outPathSize > FIOS_MAX_RESOLVE_PATH)
        return SCE_FIOS_ERROR_OUT_PATH_TOO_BIG;

    std::string resolved;
    int ret = 0;
    if (opt->maxPath > FIOS_MAX_RESOLVE_PATH) {
        ret = SCE_FIOS_ERROR_BAD_MAX_PATH;
    } else if (strnlen(pInPath, opt->maxPath) >= opt->maxPath) {
        ret = SCE_FIOS_ERROR_IN_PATH_TOO_LONG;
    } else {
        resolved = resolve_path(emuenv.io, pInPath, opt->loOrderFilter, opt->hiOrderFilter);
        if (resolved.size() >= opt->maxPath)
            ret = SCE_FIOS_ERROR_INVALID_PATH;
    }

    char *out = opt->pOutPath.get(emuenv.mem);
    if (opt->outPathSize > 0) {
        out[(ret == 0) ? resolved.copy(out, opt->outPathSize - 1) : 0] = '\0';
    }

    return ret;
}

EXPORT(int, _sceFiosKernelOverlayThreadIsDisabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiosKernelOverlayThreadSetDisabled) {
    return UNIMPLEMENTED();
}
