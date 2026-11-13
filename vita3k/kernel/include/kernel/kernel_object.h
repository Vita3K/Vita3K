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
#include <util/ref.h>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstring>
#include <map>
#include <mutex>
#include <string_view>

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

class KernelObject;

// A kernel object type with a single class, given by its static uid_class.
template <typename T>
concept KernelObjectClass = std::derived_from<T, KernelObject> && requires {
    { T::uid_class } -> std::convertible_to<UidClass>;
};

// A kernel object that guest code refers to by UID.
class KernelObject : public RefCounted {
public:
    // The object as a T, or null if it is of another class
    template <KernelObjectClass T>
    T *as() {
        return get_uid_class() == T::uid_class ? static_cast<T *>(this) : nullptr;
    }

    // Returns the class reported by sceKernelGetThreadmgrUIDClass.
    virtual UidClass get_uid_class() const = 0;
    // Whether open calls find the object by its name.
    virtual bool is_openable() const { return false; }
    // Ends what the object is doing when the thread deleter deletes it, which happens once
    virtual void on_delete(SceUID deleter) {}

    SceUID uid{};
    char name[SCE_UID_NAMELEN + 1] = {};

protected:
    explicit KernelObject(const char *name) {
        std::ranges::copy(std::string_view(name, strnlen(name, SCE_UID_NAMELEN)), this->name);
    }
};

// Base of a kernel object type with a single class, which uid_class names.
template <std::derived_from<KernelObject> Base, UidClass C>
class WithUidClass : public Base {
public:
    using Base::Base;
    static constexpr UidClass uid_class = C;
    UidClass get_uid_class() const final { return C; }
};

// The kernel objects by UID. UIDs are shared by objects of all classes.
class KernelObjects {
public:
    // Adds obj under uid. Returns uid.
    SceUID add(Ref<KernelObject> obj, SceUID uid) {
        assert(uid > 0);
        obj->uid = uid;
        const std::lock_guard<std::mutex> lock(mutex);
        objects.emplace(uid, std::move(obj));
        return uid;
    }

    // Returns the object with this UID, of any class, or null.
    [[nodiscard]] Ref<KernelObject> find(SceUID uid) const {
        const std::lock_guard<std::mutex> lock(mutex);
        const auto it = objects.find(uid);
        return it == objects.end() ? nullptr : it->second;
    }

    // Returns the openable object with this name, of any class, or null.
    [[nodiscard]] Ref<KernelObject> find_openable(const char *name) const {
        const std::lock_guard<std::mutex> lock(mutex);
        for (const auto &[_, obj] : objects) {
            if (obj->is_openable() && strncmp(obj->name, name, sizeof(obj->name)) == 0)
                return obj;
        }
        return nullptr;
    }

    // Returns the T with this UID, or null.
    template <KernelObjectClass T>
    [[nodiscard]] Ref<T> find(SceUID uid) const {
        const std::lock_guard<std::mutex> lock(mutex);
        // The reference is taken with the table locked, so the object can't go away before it
        const auto it = objects.find(uid);
        T *const object = it == objects.end() ? nullptr : it->second->template as<T>();
        return object ? Ref<T>::from(*object) : nullptr;
    }

    // Removes the T with this UID, so no later lookup finds it, then runs its on_delete() for the
    // thread deleter. Returns false if there is none.
    template <KernelObjectClass T>
    bool remove(SceUID uid, SceUID deleter) {
        Ref<KernelObject> obj;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            const auto it = objects.find(uid);
            if (it == objects.end() || !it->second->template as<T>())
                return false;
            obj = std::move(objects.extract(it).mapped());
        }
        // Without the table lock, so lookups never wait for on_delete()
        obj->on_delete(deleter);
        return true;
    }

    // Calls fn on every T, in UID order. The table isn't locked while fn runs, so fn can meet a T
    // deleted meanwhile, and fn locks the ones it reads.
    template <KernelObjectClass T>
    void for_each(std::invocable<T &> auto fn) const {
        SceUID last = 0;
        while (const Ref<KernelObject> obj = next<T>(last)) {
            last = obj->uid;
            fn(*obj->template as<T>());
        }
    }

    // Removes every object.
    void clear() {
        const std::lock_guard<std::mutex> lock(mutex);
        objects.clear();
    }

private:
    using Map = std::map<SceUID, Ref<KernelObject>>;

    // Returns the first T with a UID above last, or null.
    template <KernelObjectClass T>
    Ref<KernelObject> next(SceUID last) const {
        const std::lock_guard<std::mutex> lock(mutex);
        for (auto it = objects.upper_bound(last); it != objects.end(); ++it) {
            if (it->second->template as<T>())
                return it->second;
        }
        return nullptr;
    }

    // Held for one step on the table only, so it can be taken under any other kernel lock
    mutable std::mutex mutex;
    Map objects;
};
