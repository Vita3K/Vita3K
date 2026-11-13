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

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <utility>

// The reference count of an object held through Ref
class RefCounted {
protected:
    RefCounted() = default;
    // An object is only destroyed once nothing refers to it
    virtual ~RefCounted() {
        assert(refs.load(std::memory_order_relaxed) == 0);
    }

private:
    template <class T>
    friend class Ref;

    std::atomic<uint32_t> refs{ 0 };
};

// A counted reference to an object. T can be an incomplete type where a Ref is only named, and
// must be complete where one is copied or dropped.
template <class T>
class Ref {
public:
    // A reference to nothing
    Ref() = default;

    // A reference to nothing, from nullptr
    Ref(std::nullptr_t) {}

    // A new reference to object, which something else must keep alive during the call
    explicit Ref(T *object)
        requires std::derived_from<T, RefCounted>
        : ptr(object) {
        assert(ptr);
        retain();
    }

    // Another reference to the object of other
    Ref(const Ref &other)
        : ptr(other.ptr) {
        retain();
    }

    // Takes the reference of other over, which leaves other empty
    Ref(Ref &&other) noexcept
        : ptr(other.release_ownership()) {}

    // Another reference to the object of other, as its base class T
    template <std::derived_from<T> U>
    Ref(const Ref<U> &other)
        : ptr(other.ptr) {
        retain();
    }

    // Takes the reference of other over as its base class T, which leaves other empty
    template <std::derived_from<T> U>
    Ref(Ref<U> &&other) noexcept
        : ptr(other.release_ownership()) {}

    // Refers to the object of other and drops the reference held before
    Ref &operator=(Ref other) noexcept {
        std::swap(ptr, other.ptr);
        return *this;
    }

    // Drops the reference
    ~Ref() {
        release();
    }

    // Creates an object and returns the first reference to it
    template <class... Args>
    [[nodiscard]] static Ref make(Args &&...args)
        requires std::derived_from<T, RefCounted>
    {
        return Ref(new T(std::forward<Args>(args)...));
    }

    // The object, or null
    T *get() const {
        return ptr;
    }

    // The object, which must be there
    T &operator*() const {
        assert(ptr);
        return *ptr;
    }

    // The object for a member access, which must be there
    T *operator->() const {
        assert(ptr);
        return ptr;
    }

    // Whether it refers to an object
    explicit operator bool() const {
        return ptr != nullptr;
    }

    // Whether both refer to the same object
    bool operator==(const Ref &) const = default;

    // Whether it refers to nothing
    bool operator==(std::nullptr_t) const {
        return ptr == nullptr;
    }

private:
    template <class U>
    friend class Ref;

    // Adds this reference to the object's count
    void retain() noexcept {
        if (ptr)
            counted()->refs.fetch_add(1, std::memory_order_relaxed);
    }

    // Drops this reference and destroys the object with its last one
    void release() noexcept {
        if (!ptr)
            return;
        const uint32_t old = counted()->refs.fetch_sub(1, std::memory_order_acq_rel);
        assert(old > 0);
        if (old == 1)
            delete counted();
    }

    // The object as the holder of its count
    RefCounted *counted() const noexcept {
        return ptr;
    }

    // Gives the reference up without dropping it
    [[nodiscard]] T *release_ownership() noexcept {
        return std::exchange(ptr, nullptr);
    }

    T *ptr = nullptr;
};
