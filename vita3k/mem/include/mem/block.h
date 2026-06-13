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

#pragma once

#include <mem/util.h>

#include <cstdint>
#include <utility>

template <typename T>
class Ptr;

struct MemState;

class Block {
public:
    Block() = default;
    Block(std::nullptr_t) noexcept {
    }

    Block(const Block &) = delete;
    Block &operator=(const Block &) = delete;

    Block(Block &&other) noexcept;
    Block &operator=(Block &&other) noexcept;

    ~Block();

    static Block allocate(MemState &mem, uint32_t requested_size, const char *name, Address start_addr);
    static Block bind_existing(MemState &mem, Address addr, uint32_t requested_size, uint32_t committed_size);

    Block &operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return addr_ != 0;
    }

    [[nodiscard]] Address get() const noexcept {
        return addr_;
    }

    template <typename T>
    [[nodiscard]] Ptr<T> get_ptr() const {
        return Ptr<T>(addr_);
    }

    [[nodiscard]] uint32_t size() const noexcept {
        return requested_size_;
    }

    [[nodiscard]] uint32_t committed_size() const noexcept {
        return committed_size_;
    }

    [[nodiscard]] bool owns_memory() const noexcept {
        return owns_;
    }

    [[nodiscard]] Address release() noexcept;

    void reset() noexcept;

private:
    Block(MemState *mem, Address addr, uint32_t requested_size, uint32_t committed_size, bool owns) noexcept
        : mem_(mem)
        , addr_(addr)
        , requested_size_(requested_size)
        , committed_size_(committed_size)
        , owns_(owns) {
    }

    MemState *mem_ = nullptr;
    Address addr_ = 0;
    uint32_t requested_size_ = 0;
    uint32_t committed_size_ = 0;
    bool owns_ = false;
};
