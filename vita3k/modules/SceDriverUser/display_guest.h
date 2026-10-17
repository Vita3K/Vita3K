// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "../SceDisplay/SceDisplay.h"
#include "../SceKernelThreadMgr/lightweight.h"

#include <expected>

namespace display_guest {

inline std::expected<SceDisplayFrameBuf, SceDisplayErrorCode> read_frame(MemState &mem, Ptr<const SceDisplayFrameBuf> address) {
    SceDisplayFrameBuf frame{};
    if (!address || !threadmgr::copy_sync_guest_bytes(mem, address.address(), &frame, sizeof(frame), false))
        return std::unexpected(SCE_DISPLAY_ERROR_INVALID_ADDR);
    if (frame.size != sizeof(SceDisplayFrameBuf) && frame.size != sizeof(SceDisplayFrameBuf2))
        return std::unexpected(SCE_DISPLAY_ERROR_INVALID_VALUE);
    if (frame.size == sizeof(SceDisplayFrameBuf2)) {
        SceDisplayFrameBuf2 extended{};
        if (!threadmgr::copy_sync_guest_bytes(mem, address.address(), &extended, sizeof(extended), false))
            return std::unexpected(SCE_DISPLAY_ERROR_INVALID_ADDR);
        if (extended.size != sizeof(extended))
            return std::unexpected(SCE_DISPLAY_ERROR_INVALID_VALUE);
        frame = extended;
    }
    return frame;
}

} // namespace display_guest