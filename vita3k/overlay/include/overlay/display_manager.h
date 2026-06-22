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
// Copyright RPCS3
// SPDX-License-Identifier: GPL-2.0
// Code heavily referenced/taken from https://github.com/RPCS3/rpcs3/tree/master/rpcs3/Emu/RSX/Overlays

#pragma once

#include <overlay/user_interface.h>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <vector>

namespace overlay {

// Context for an overlay that requires interactive input processing.
struct input_thread_context_t {
    std::string_view name;
    std::shared_ptr<user_interface> target;
    std::function<void()> input_loop_prologue;
    std::function<void(int32_t)> input_loop_epilogue;
    std::function<int32_t()> input_loop_override;
    bool prologue_completed = false;
};

class display_manager {
private:
    std::atomic<uint32_t> m_uid_ctr{ 0 };
    std::vector<std::shared_ptr<overlay>> m_iface_list;
    std::vector<std::shared_ptr<overlay>> m_dirty_list;

    mutable std::shared_mutex m_list_mutex;

    std::queue<uint32_t> m_uids_to_remove;
    std::queue<uint32_t> m_type_ids_to_remove;
    std::mutex m_removal_queue_mutex;
    std::atomic<uint32_t> m_pending_removals_count{ 0 };
    std::shared_ptr<void> m_alive_sentinel = std::make_shared<char>(0);

    bool remove_type(uint32_t type_id);
    bool remove_uid(uint32_t uid);
    void cleanup_internal();

    void on_overlay_activated(const std::shared_ptr<overlay> &item);
    void on_overlay_removed(const std::shared_ptr<overlay> &item);

    poll_input_callback_t m_poll_input;
    set_intercept_callback_t m_set_intercepted;
    poll_touch_callback_t m_poll_touch;

    std::thread m_input_thread;
    std::deque<input_thread_context_t> m_input_token_stack;
    std::atomic<bool> m_input_thread_abort{ false };
    std::atomic<bool> m_input_thread_interrupted{ false };
    std::atomic<bool> m_paused{ false };
    std::mutex m_input_stack_guard;
    std::condition_variable m_input_stack_cv;

    // Callback to request an async present from the render thread.
    std::function<void()> m_flip_request;

    void input_thread_loop();

public:
    display_manager();
    ~display_manager();

    // Provide the concrete input polling and interception callbacks.
    // Must be called before any interactive overlay is attached.
    void set_input_callbacks(poll_input_callback_t poll_fn, set_intercept_callback_t intercept_fn,
        poll_touch_callback_t touch_fn = nullptr);

    // Set the callback that overlays use to request async presents from the render thread.
    void set_flip_request_callback(std::function<void()> cb);

    // Pause/unpause all overlay input processing.
    void set_paused(bool paused) { m_paused.store(paused, std::memory_order_relaxed); }
    bool is_paused() const { return m_paused.load(std::memory_order_relaxed); }

    // Push an interactive overlay onto the input thread for processing.
    void attach_thread_input(
        const std::string_view &name,
        std::shared_ptr<user_interface> target,
        std::function<void()> on_input_loop_enter = nullptr,
        std::function<void(int32_t)> on_input_loop_exit = nullptr,
        std::function<int32_t()> input_loop_override = nullptr);

    // Adds an object to the internal list. Optionally removes other objects of the same type.
    // Original handle loses ownership but a usable pointer is returned.
    template <typename T>
    std::shared_ptr<T> add(std::shared_ptr<T> &entry, bool remove_existing = true) {
        std::lock_guard lock(m_list_mutex);

        entry->uid = m_uid_ctr.fetch_add(1);
        entry->type_index = get_overlay_type_id<T>();

        if (remove_existing) {
            for (auto it = m_iface_list.begin(); it != m_iface_list.end(); ++it) {
                if ((*it)->type_index == entry->type_index) {
                    on_overlay_removed(*it);
                    m_dirty_list.push_back(std::move(*it));
                    *it = std::move(entry);
                    on_overlay_activated(*it);
                    return std::static_pointer_cast<T>(*it);
                }
            }
        }

        m_iface_list.push_back(std::move(entry));
        on_overlay_activated(m_iface_list.back());
        return std::static_pointer_cast<T>(m_iface_list.back());
    }

    // Allocates object and adds to internal list. Returns pointer to created object.
    template <typename T, typename... Args>
    std::shared_ptr<T> create(Args &&...args) {
        auto object = std::make_shared<T>(std::forward<Args>(args)...);
        return add(object);
    }

    void remove(uint32_t uid);

    // Removes all objects of this type from the list
    template <typename T>
    void remove() {
        const auto type_id = get_overlay_type_id<T>();

        {
            std::unique_lock lock(m_list_mutex, std::try_to_lock);
            if (lock.owns_lock()) {
                remove_type(type_id);
                return;
            }
        }

        // Enqueue for later removal
        {
            std::lock_guard qlock(m_removal_queue_mutex);
            m_type_ids_to_remove.push(type_id);
        }
        m_pending_removals_count++;
    }

    // True if any visible elements to draw exist.
    // Caller must ensure synchronization by first locking the list.
    bool has_visible() const {
        return std::any_of(m_iface_list.begin(), m_iface_list.end(), [](const auto &iface) {
            return iface && iface->visible.load(std::memory_order_relaxed);
        });
    }

    // True if any elements have been deleted but their resources may not have been cleaned up.
    // Caller must ensure synchronization by first locking the list.
    bool has_dirty() const {
        return !m_dirty_list.empty();
    }

    // Returns current list for reading. Caller must ensure synchronization by first locking the list.
    const std::vector<std::shared_ptr<overlay>> &get_views() const {
        return m_iface_list;
    }

    // Returns current list of removed objects not yet deallocated for reading.
    const std::vector<std::shared_ptr<overlay>> &get_dirty() const {
        return m_dirty_list;
    }

    // Deallocate objects. Objects must first be removed via the remove() functions.
    void dispose(const std::vector<uint32_t> &uids);

    // Returns pointer to the object matching the given uid
    std::shared_ptr<overlay> get(uint32_t uid) const;

    // Returns pointer to the first object matching the given type
    template <typename T>
    std::shared_ptr<T> get() const {
        std::shared_lock lock(m_list_mutex);

        const auto type_id = get_overlay_type_id<T>();
        for (const auto &iface : m_iface_list) {
            if (iface->type_index == type_id) {
                return std::static_pointer_cast<T>(iface);
            }
        }

        return {};
    }

    // Lock for exclusive access (BasicLockable)
    void lock();

    // Release lock. May perform internal cleanup before returning.
    void unlock();

    // Lock for shared access
    void lock_shared() const;

    // Unlock shared access
    void unlock_shared() const;
};

} // namespace overlay
