// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <audio/state.h>
#include <mem/functions.h>
#include <mem/ptr.h>

#include <algorithm>
#include <cstring>
#include <span>

namespace audio_process {

inline AudioOutPortPtr find_port(AudioState &audio, int port_id, int32_t pid) {
    const std::lock_guard lock(audio.mutex);
    const auto it = audio.out_ports.find(port_id);
    return it != audio.out_ports.end() && it->second->owner_pid == pid ? it->second : nullptr;
}

inline bool release_port(AudioState &audio, int port_id, int32_t pid) {
    const std::lock_guard lock(audio.mutex);
    const auto it = audio.out_ports.find(port_id);
    if (it == audio.out_ports.end() || it->second->owner_pid != pid)
        return false;
    it->second->stopping = true;
    if (audio.adapter)
        audio.adapter->wake_all_ports();
    audio.out_ports.erase(it);
    return true;
}

inline bool copy_guest_buffer(MemState &mem, Address address, void *destination, uint32_t size) {
    if (!size)
        return true;
    const GuestMemoryRange range{ address, size };
    const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
    if (!pin)
        return false;
    auto *bytes = static_cast<uint8_t *>(destination);
    uint32_t offset = 0;
    while (offset < size) {
        const Address current = address + offset;
        const uint32_t count = std::min(size - offset, uint32_t(4096 - current % 4096));
        std::memcpy(bytes + offset, Ptr<const uint8_t>(current).get(mem), count);
        offset += count;
    }
    unpin_guest_memory(mem, pin);
    return true;
}

} // namespace audio_process