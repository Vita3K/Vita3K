// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <functional>
#include <mem/util.h>

template <typename T>
class Ptr;

class Block {
public:
    typedef std::function<void(Address)> Deleter;

    explicit Block(Address addr, Deleter deleter)
        : addr(addr)
        , deleter(deleter) {
    }

    Block()
        : addr(0)
        , deleter(nullptr) {
    }

    Block(std::nullptr_t)
        : addr(0)
        , deleter(nullptr) {
    }

    Block &operator=(std::nullptr_t) {
        if (deleter) {
            deleter(addr);
        }
        addr = 0;
        deleter = nullptr;
        return *this;
    }

    ~Block() {
        if (deleter) {
            deleter(addr);
        }
    }

    operator bool() const {
        return addr != 0;
    }

    Address get() const {
        return addr;
    }

    template <class T>
    Ptr<T> get_ptr() const {
        return Ptr<T>(addr);
    }

    Block(Block &&o) noexcept {
        std::swap(addr, o.addr);
        std::swap(deleter, o.deleter);
    }

    Block &operator=(Block &&o) noexcept {
        std::swap(addr, o.addr);
        std::swap(deleter, o.deleter);
        return *this;
    }

private:
    Block(const Block &) = delete;
    const Block &operator=(const Block &) = delete;

    Address addr;
    Deleter deleter;
};