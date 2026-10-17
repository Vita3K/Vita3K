// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

class AudioBufferQueue {
    struct Buffer {
        std::vector<uint8_t> data;
        size_t position = 0;
    };

    std::mutex mutex;
    std::condition_variable available;
    std::vector<Buffer> buffers;
    size_t block_size = 0;
    size_t next = 0;
    size_t ready = 0;

public:
    void configure(size_t count, size_t size) {
        buffers.resize(count);
        for (auto &buffer : buffers)
            buffer.data.resize(size);
        block_size = size;
    }

    bool push(const void *data, const std::atomic<bool> &stopping) {
        std::unique_lock lock(mutex);
        available.wait(lock, [&] { return stopping.load() || ready < buffers.size(); });
        if (stopping.load())
            return false;
        auto &buffer = buffers[(next + ready) % buffers.size()];
        std::memcpy(buffer.data.data(), data, block_size);
        buffer.position = 0;
        ++ready;
        return true;
    }

    size_t pop(void *output, size_t size) {
        auto *bytes = static_cast<uint8_t *>(output);
        size_t copied = 0;
        bool released = false;
        {
            const std::lock_guard lock(mutex);
            while (copied < size && ready) {
                auto &buffer = buffers[next];
                const size_t count = std::min(size - copied, block_size - buffer.position);
                std::memcpy(bytes + copied, buffer.data.data() + buffer.position, count);
                copied += count;
                buffer.position += count;
                if (buffer.position == block_size) {
                    next = (next + 1) % buffers.size();
                    --ready;
                    released = true;
                }
            }
        }
        if (released)
            available.notify_all();
        std::memset(bytes + copied, 0, size - copied);
        return copied;
    }

    void wake() {
        {
            const std::lock_guard lock(mutex);
        }
        available.notify_all();
    }
};