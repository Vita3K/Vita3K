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

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string_view>
#include <vector>

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
    explicit KernelObject(const char *name) {
        std::ranges::copy(std::string_view(name, strnlen(name, SCE_UID_NAMELEN)), this->name);
    }
    virtual ~KernelObject() = default;
    // Returns the class reported by sceKernelGetThreadmgrUIDClass.
    virtual UidClass get_uid_class() const = 0;

    SceUID uid{};
    char name[SCE_UID_NAMELEN + 1] = {};

protected:
    // Locks the object, which guards its fields. The lock is empty if the object was deleted. Only
    // the object's own methods take it.
    [[nodiscard]] std::unique_lock<std::mutex> lock() const {
        std::unique_lock<std::mutex> guard(mutex);
        if (deleted)
            guard.unlock();
        return guard;
    }

    // Ends what the object is doing when the thread deleter deletes it. Runs once, with the lock held.
    virtual void on_delete(SceUID deleter) {}
    virtual void on_delete(SceUID deleter, SceUID process_id) { on_delete(deleter); }

    // Whether the object was deleted. With the lock held.
    bool was_deleted() const { return deleted; }

private:
    friend class KernelObjects;

    // Marks the object deleted by the thread deleter and runs on_delete().
    void mark_deleted(SceUID deleter, SceUID process_id = 0) {
        const std::lock_guard<std::mutex> guard(mutex);
        assert(!deleted);
        deleted = true;
        on_delete(deleter, process_id);
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

constexpr bool event_flag_legacy_uid_rules(SceUInt32 sdk_version) {
    return sdk_version != 0 && sdk_version < 0x03100000;
}

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
        const auto it = find_handle(uid);
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

    // Removes the T with this UID, so no later lookup finds it, then marks it deleted by the thread
    // deleter. Returns false if there is none.
    template <KernelObjectClass T>
    bool remove(SceUID uid, SceUID deleter, SceUID process_id = 0) {
        static_assert(T::uid_class != UidClass::event_flag);
        std::shared_ptr<KernelObject> obj;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            const auto it = lookup<T>(uid);
            if (it == objects.end())
                return false;
            obj = std::move(objects.extract(it).mapped());
        }
        // Without the table lock, so lookups never wait for on_delete()
        obj->mark_deleted(deleter, process_id);
        return true;
    }

    // Opens a separate event-flag UID while holding the table lock, so the object cannot disappear
    // between the name lookup and creation of the reference.
    template <KernelObjectClass T>
    bool open_event_flag_reference(SceUID uid, std::predicate<const T &> auto pick) {
        static_assert(T::uid_class == UidClass::event_flag);
        const std::lock_guard<std::mutex> lock(mutex);
        assert(uid > 0 && !objects.contains(uid) && !event_flag_references.contains(uid));
        if (uid <= 0 || objects.contains(uid) || event_flag_references.contains(uid))
            return false;
        for (const auto &[owner, obj] : objects) {
            if (obj->get_uid_class() != T::uid_class)
                continue;
            const std::lock_guard<std::mutex> obj_lock(obj->mutex);
            if (!pick(static_cast<const T &>(*obj)))
                continue;
            event_flag_references.emplace(uid, owner);
            ++event_flag_reference_counts[owner];
            return true;
        }
        return false;
    }

    template <KernelObjectClass T>
    [[nodiscard]] std::pair<std::shared_ptr<T>, bool> find_event_flag_reference(SceUID uid) const {
        static_assert(T::uid_class == UidClass::event_flag);
        const std::lock_guard<std::mutex> lock(mutex);
        const auto it = lookup<T>(uid);
        if (it == objects.end())
            return { nullptr, false };
        return { std::static_pointer_cast<T>(it->second), event_flag_references.contains(uid) };
    }

    // Removes one event-flag UID. Older SDKs accept either kind of UID for close or delete.
    bool remove_event_flag_reference(SceUID uid, SceUID deleter, bool close, bool legacy) {
        std::shared_ptr<KernelObject> deleted;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            const auto reference = event_flag_references.find(uid);
            const bool opened = reference != event_flag_references.end();
            const SceUID owner = opened ? reference->second : uid;
            const auto it = objects.find(owner);
            if (it == objects.end() || it->second->get_uid_class() != UidClass::event_flag
                || (!opened && closed_event_flag_owners.contains(owner)) || (!legacy && close != opened))
                return false;

            if (opened) {
                event_flag_references.erase(reference);
                const auto count = event_flag_reference_counts.find(owner);
                assert(count != event_flag_reference_counts.end() && count->second > 0);
                if (--count->second == 0)
                    event_flag_reference_counts.erase(count);
            } else {
                closed_event_flag_owners.insert(owner);
            }
            if (closed_event_flag_owners.contains(owner) && !event_flag_reference_counts.contains(owner)) {
                closed_event_flag_owners.erase(owner);
                deleted = std::move(objects.extract(it).mapped());
            }
        }
        if (deleted)
            deleted->mark_deleted(deleter);
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

    // Calls fn on every T, in UID order, with the table locked. No T is deleted meanwhile, and fn
    // locks the ones it reads.
    template <KernelObjectClass T>
    void for_each(std::invocable<T &> auto fn) const {
        const std::lock_guard<std::mutex> lock(mutex);
        for (const auto &[_, obj] : objects) {
            if (obj->get_uid_class() == T::uid_class)
                fn(static_cast<T &>(*obj));
        }
    }

    template <KernelObjectClass T>
    void remove_if(std::predicate<const T &> auto pick, SceUID deleter) {
        std::vector<std::shared_ptr<KernelObject>> removed;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            for (auto it = objects.begin(); it != objects.end();) {
                if (it->second->get_uid_class() == T::uid_class && pick(static_cast<const T &>(*it->second))) {
                    removed.push_back(std::move(it->second));
                    it = objects.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (const auto &obj : removed)
            obj->mark_deleted(deleter);
    }

    // Removes every object.
    void clear() {
        const std::lock_guard<std::mutex> lock(mutex);
        objects.clear();
        event_flag_references.clear();
        event_flag_reference_counts.clear();
        closed_event_flag_owners.clear();
    }

private:
    using Map = std::map<SceUID, std::shared_ptr<KernelObject>>;

    Map::const_iterator find_handle(SceUID uid) const {
        if (const auto reference = event_flag_references.find(uid); reference != event_flag_references.end())
            return objects.find(reference->second);
        if (closed_event_flag_owners.contains(uid))
            return objects.end();
        return objects.find(uid);
    }

    // Returns the entry of the T with this UID, or end(). mutex must be held.
    template <KernelObjectClass T>
    Map::const_iterator lookup(SceUID uid) const {
        const auto it = find_handle(uid);
        if (it == objects.end() || it->second->get_uid_class() != T::uid_class)
            return objects.end();
        return it;
    }

    mutable std::mutex mutex;
    Map objects;
    std::map<SceUID, SceUID> event_flag_references;
    std::map<SceUID, size_t> event_flag_reference_counts;
    std::set<SceUID> closed_event_flag_owners;
};
