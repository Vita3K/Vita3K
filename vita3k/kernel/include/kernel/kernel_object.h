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

#include <kernel/types.h>

#include <cassert>
#include <concepts>
#include <map>
#include <memory>
#include <mutex>

// Thread manager object classes, with the values sceKernelGetThreadmgrUIDClass reports
enum class UidClass : SceUInt32 {
    thread = 1,
    semaphore = 2,
    event_flag = 3,
    mutex = 4,
    cond = 5,
    timer = 6,
    msg_pipe = 7,
    callback = 8,
    thread_event = 9,
    lw_mutex = 10,
    lw_cond = 11,
    rw_lock = 12,
    simple_event = 13,
};

// A kernel object that guest code refers to by UID.
class KernelObject {
public:
    virtual ~KernelObject() = default;
    // Returns the class reported by sceKernelGetThreadmgrUIDClass.
    virtual UidClass get_uid_class() const = 0;

    SceUID uid{};

    // Locks the object, which guards its fields. The lock is empty if the object was deleted.
    [[nodiscard]] std::unique_lock<std::mutex> lock() const {
        std::unique_lock<std::mutex> guard(mutex);
        if (deleted)
            guard.unlock();
        return guard;
    }

protected:
    // Ends what the object is doing when it is deleted. Runs once, with the lock held.
    virtual void on_delete() {}

private:
    friend class KernelObjects;

    // Marks the object deleted and runs on_delete().
    void mark_deleted() {
        const std::lock_guard<std::mutex> guard(mutex);
        assert(!deleted);
        deleted = true;
        on_delete();
    }

    mutable std::mutex mutex;
    bool deleted = false;
};

// Base of a kernel object type with a single class, which uid_class names.
template <std::derived_from<KernelObject> Base, UidClass C>
class WithUidClass : public Base {
public:
    using Base::Base;
    static constexpr UidClass uid_class = C;
    UidClass get_uid_class() const final { return C; }
};

// A kernel object type with a single class, given by its static uid_class.
template <typename T>
concept KernelObjectClass = std::derived_from<T, KernelObject> && std::same_as<decltype(T::uid_class), const UidClass>;

// The kernel objects by UID. UIDs are shared by objects of all classes.
class KernelObjects {
public:
    // Adds obj under uid. Returns uid.
    SceUID add(std::shared_ptr<KernelObject> obj, SceUID uid) {
        assert(uid > 0);
        obj->uid = uid;
        const std::lock_guard<std::mutex> lock(mutex);
        objects.emplace(uid, std::move(obj));
        return uid;
    }

    // Returns the object with this UID, of any class, or null.
    [[nodiscard]] std::shared_ptr<KernelObject> find(SceUID uid) const {
        const std::lock_guard<std::mutex> lock(mutex);
        const auto it = objects.find(uid);
        return it == objects.end() ? nullptr : it->second;
    }

    // Returns the T with this UID, or null.
    template <KernelObjectClass T>
    [[nodiscard]] std::shared_ptr<T> find(SceUID uid) const {
        const std::lock_guard<std::mutex> lock(mutex);
        const auto it = lookup<T>(uid);
        if (it == objects.end())
            return nullptr;
        return std::static_pointer_cast<T>(it->second);
    }

    // Removes the T with this UID and marks it deleted. Returns false if there is none.
    template <KernelObjectClass T>
    bool remove(SceUID uid) {
        std::shared_ptr<KernelObject> obj;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            const auto it = lookup<T>(uid);
            if (it == objects.end())
                return false;
            obj = std::move(objects.extract(it).mapped());
        }
        // Without the table lock, so lookups never wait for on_delete()
        obj->mark_deleted();
        return true;
    }

    // Returns the first T, in UID order, that pick accepts, or null. pick runs with the table and the object locked.
    template <KernelObjectClass T>
    [[nodiscard]] std::shared_ptr<T> find_if(std::predicate<const T &> auto pick) const {
        const std::lock_guard<std::mutex> lock(mutex);
        for (const auto &[_, obj] : objects) {
            if (obj->get_uid_class() != T::uid_class)
                continue;
            const std::lock_guard<std::mutex> obj_lock(obj->mutex);
            if (pick(static_cast<const T &>(*obj)))
                return std::static_pointer_cast<T>(obj);
        }
        return nullptr;
    }

    // Calls fn on every T, in UID order, with the table and the object locked.
    template <KernelObjectClass T>
    void for_each(std::invocable<T &> auto fn) const {
        const std::lock_guard<std::mutex> lock(mutex);
        for (const auto &[_, obj] : objects) {
            if (obj->get_uid_class() != T::uid_class)
                continue;
            const std::lock_guard<std::mutex> obj_lock(obj->mutex);
            fn(static_cast<T &>(*obj));
        }
    }

    // Removes every object.
    void clear() {
        const std::lock_guard<std::mutex> lock(mutex);
        objects.clear();
    }

private:
    using Map = std::map<SceUID, std::shared_ptr<KernelObject>>;

    // Returns the entry of the T with this UID, or end(). mutex must be held.
    template <KernelObjectClass T>
    Map::const_iterator lookup(SceUID uid) const {
        const auto it = objects.find(uid);
        if (it == objects.end() || it->second->get_uid_class() != T::uid_class)
            return objects.end();
        return it;
    }

    mutable std::mutex mutex;
    Map objects;
};
