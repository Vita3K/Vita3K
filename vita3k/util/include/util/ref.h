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

#include <util/intrusive_list.h>

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

    std::atomic<uint32_t> refs{ 1 };
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
    [[nodiscard]] static Ref from(T &object)
        requires std::derived_from<T, RefCounted>
    {
        Ref ref;
        ref.ptr = &object;
        ref.retain();
        return ref;
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
        return adopt(new T(std::forward<Args>(args)...));
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

    template <class U>
    friend class RefList;

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

    // Takes over a reference given up with release_ownership()
    [[nodiscard]] static Ref adopt(T *object) noexcept {
        Ref ref;
        ref.ptr = object;
        return ref;
    }

    T *ptr = nullptr;
};

// An intrusive list that holds one reference to each object on it. The caller locks it.
template <class T>
class RefList {
public:
    // Drops the reference to every object still on the list
    ~RefList() {
        while (!list.empty())
            remove(*list.begin());
    }

    // Whether no object is on the list
    bool empty() const {
        return list.empty();
    }

    // The objects in list order
    auto begin() {
        return list.begin();
    }

    // The end of the objects in list order
    auto end() {
        return list.end();
    }

    // A new reference to the first object, or nothing if the list is empty
    Ref<T> front() {
        if (list.empty())
            return nullptr;
        return Ref<T>::from(list.front());
    }

    // Links object before position and takes its reference over.
    void insert(typename IntrusiveList<T>::iterator position, Ref<T> object) {
        list.insert(position, *object.release_ownership());
    }

    // Links object at the end and takes its reference over
    void push_back(Ref<T> object) {
        list.push_back(*object.release_ownership());
    }

    // Takes object off the list and returns the list's reference to it, or nothing if it isn't on a
    // list. object must not be on another list.
    Ref<T> remove(T &object) {
        if (!object.linked())
            return nullptr;
        list.remove(object);
        return Ref<T>::adopt(&object);
    }

private:
    IntrusiveList<T> list;
};
