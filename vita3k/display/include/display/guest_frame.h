// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <display/state.h>
#include <mem/functions.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <span>

namespace display {

// Keep the pitched guest range pinned until its visible pixels are copied.
inline bool capture_software_frame(MemState &mem, DisplayFrameInfo &frame) {
    if (!frame.base || frame.image_size.x <= 0 || frame.image_size.y <= 0
        || frame.image_size.x > 1920 || frame.image_size.y > 1088
        || frame.pitch < static_cast<uint32_t>(frame.image_size.x))
        return false;

    const uint64_t row_size = uint64_t(frame.image_size.x) * sizeof(uint32_t);
    const uint64_t stride = uint64_t(frame.pitch) * sizeof(uint32_t);
    const uint64_t source_size = (uint64_t(frame.image_size.y) - 1) * stride + row_size;
    const uint64_t output_size = uint64_t(frame.image_size.y) * row_size;
    if (source_size > std::numeric_limits<uint32_t>::max()
        || output_size > std::numeric_limits<uint32_t>::max()
        || uint64_t(frame.base.address()) + source_size > (uint64_t{ 1 } << 32))
        return false;

    const GuestMemoryRange range{ frame.base.address(), static_cast<uint32_t>(source_size) };
    const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
    if (!pin)
        return false;
    struct PinGuard {
        MemState &mem;
        uint64_t pin;
        ~PinGuard() { unpin_guest_memory(mem, pin); }
    } guard{ mem, pin };

    auto pixels = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(output_size));
    for (uint32_t y = 0; y < static_cast<uint32_t>(frame.image_size.y); ++y) {
        uint32_t copied = 0;
        const Address row = static_cast<Address>(uint64_t(frame.base.address()) + uint64_t(y) * stride);
        while (copied < row_size) {
            const Address current = row + copied;
            const uint32_t count = std::min(static_cast<uint32_t>(row_size) - copied, uint32_t(4096 - current % 4096));
            std::memcpy(pixels->data() + size_t(y) * static_cast<size_t>(row_size) + copied,
                Ptr<const uint8_t>(current).get(mem), count);
            copied += count;
        }
    }
    frame.software_pixels = std::move(pixels);
    return true;
}

} // namespace display