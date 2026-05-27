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

#include <overlay/display_manager.h>

#include <algorithm>
#include <cassert>
#include <util/log.h>

namespace overlay {

display_manager::display_manager() = default;

display_manager::~display_manager() {
    m_input_thread_abort.store(true);
    m_input_thread_interrupted.store(true);

    m_input_stack_cv.notify_all();
    if (m_input_thread.joinable())
        m_input_thread.join();

    std::lock_guard lock(m_list_mutex);
    m_iface_list.clear();
    m_dirty_list.clear();
}

void display_manager::lock() {
    m_list_mutex.lock();
}

void display_manager::unlock() {
    if (m_pending_removals_count > 0) {
        cleanup_internal();
    }

    m_list_mutex.unlock();
}

void display_manager::lock_shared() const {
    m_list_mutex.lock_shared();
}

void display_manager::unlock_shared() const {
    m_list_mutex.unlock_shared();
}

std::shared_ptr<overlay> display_manager::get(uint32_t uid) const {
    std::shared_lock lock(m_list_mutex);

    for (const auto &iface : m_iface_list) {
        if (iface->uid == uid)
            return iface;
    }

    return {};
}

void display_manager::remove(uint32_t uid) {
    {
        std::unique_lock lock(m_list_mutex, std::try_to_lock);
        if (lock.owns_lock()) {
            remove_uid(uid);
            return;
        }
    }

    // Enqueue for later removal
    {
        std::lock_guard qlock(m_removal_queue_mutex);
        m_uids_to_remove.push(uid);
    }
    m_pending_removals_count++;
}

void display_manager::dispose(const std::vector<uint32_t> &uids) {
    std::lock_guard lock(m_list_mutex);

    if (m_pending_removals_count > 0) {
        cleanup_internal();
    }

    m_dirty_list.erase(
        std::remove_if(m_dirty_list.begin(), m_dirty_list.end(),
            [&uids](const std::shared_ptr<overlay> &e) {
                return std::find(uids.begin(), uids.end(), e->uid) != uids.end();
            }),
        m_dirty_list.end());
}

bool display_manager::remove_type(uint32_t type_id) {
    bool found = false;
    for (auto it = m_iface_list.begin(); it != m_iface_list.end();) {
        if ((*it)->type_index == type_id) {
            on_overlay_removed(*it);
            m_dirty_list.push_back(std::move(*it));
            it = m_iface_list.erase(it);
            found = true;
        } else {
            ++it;
        }
    }

    return found;
}

bool display_manager::remove_uid(uint32_t uid) {
    for (auto it = m_iface_list.begin(); it != m_iface_list.end(); ++it) {
        if ((*it)->uid == uid) {
            on_overlay_removed(*it);
            m_dirty_list.push_back(std::move(*it));
            m_iface_list.erase(it);
            return true;
        }
    }

    return false;
}

void display_manager::cleanup_internal() {
    std::lock_guard qlock(m_removal_queue_mutex);

    while (!m_uids_to_remove.empty()) {
        remove_uid(m_uids_to_remove.front());
        m_uids_to_remove.pop();
        m_pending_removals_count--;
    }

    while (!m_type_ids_to_remove.empty()) {
        remove_type(m_type_ids_to_remove.front());
        m_type_ids_to_remove.pop();
        m_pending_removals_count--;
    }
}

void display_manager::on_overlay_activated(const std::shared_ptr<overlay> &item) {
    if (m_flip_request)
        item->flip_request = m_flip_request;

    auto iface = std::dynamic_pointer_cast<user_interface>(item);
    if (iface) {
        std::weak_ptr<void> weak_sentinel = m_alive_sentinel;
        display_manager *mgr = this;
        iface->request_remove = [mgr, weak_sentinel](uint32_t remove_uid) {
            if (weak_sentinel.lock()) {
                mgr->remove(remove_uid);
            }
        };
        if (m_set_intercepted) {
            iface->release_intercept = [cb = m_set_intercepted]() { cb(false); };
        }
    }
}

void display_manager::on_overlay_removed(const std::shared_ptr<overlay> &item) {
    auto iface = std::dynamic_pointer_cast<user_interface>(item);
    if (!iface) {
        return;
    }

    iface->stop_input_processing(true);
    iface->detach_input();
}

void display_manager::set_input_callbacks(poll_input_callback_t poll_fn, set_intercept_callback_t intercept_fn,
    poll_touch_callback_t touch_fn) {
    m_poll_input = std::move(poll_fn);
    m_set_intercepted = std::move(intercept_fn);
    m_poll_touch = std::move(touch_fn);
}

void display_manager::set_flip_request_callback(std::function<void()> cb) {
    m_flip_request = std::move(cb);
}

void display_manager::attach_thread_input(
    const std::string_view &name,
    std::shared_ptr<user_interface> target,
    std::function<void()> on_input_loop_enter,
    std::function<void(int32_t)> on_input_loop_exit,
    std::function<int32_t()> input_loop_override) {
    {
        std::lock_guard lock(m_input_stack_guard);

        // Lazily start the input thread on first attach.
        if (!m_input_thread.joinable()) {
            m_input_thread_abort.store(false);
            m_input_thread = std::thread([this]() { input_thread_loop(); });
        }

        m_input_token_stack.push_back({ name,
            std::move(target),
            std::move(on_input_loop_enter),
            std::move(on_input_loop_exit),
            std::move(input_loop_override),
            false });
    }

    // Signal input thread loop after pushing to avoid a race.
    m_input_thread_interrupted.store(true);
    m_input_stack_cv.notify_one();
}

void display_manager::input_thread_loop() {
    // Avoid tail recursion by reinserting pushed-down items.
    std::vector<input_thread_context_t> interrupted_items;

    while (!m_input_thread_abort) {
        // We're about to load the whole list, interruption makes no sense before this point.
        m_input_thread_interrupted.store(false);

        std::deque<input_thread_context_t> batch;
        {
            std::lock_guard lock(m_input_stack_guard);
            batch.swap(m_input_token_stack);
        }

        for (auto &input_context : batch) {
            if (!input_context.target || input_context.target->is_detached()) {
                continue;
            }

            if (m_input_thread_interrupted.load()) {
                // Someone just pushed something onto the stack. Check if we actually have new items.
                bool has_new;
                {
                    std::lock_guard lock(m_input_stack_guard);
                    has_new = !m_input_token_stack.empty();
                }

                if (has_new) {
                    // We actually have new items to read out. Skip the remaining list.
                    interrupted_items.push_back(std::move(input_context));
                    continue;
                }

                // False alarm, we already saw it.
                m_input_thread_interrupted.store(false);
            }

            if (!input_context.prologue_completed && input_context.input_loop_prologue) {
                input_context.input_loop_prologue();
                input_context.prologue_completed = true;
            }

            int32_t result;
            if (input_context.input_loop_override) {
                result = input_context.input_loop_override();
            } else {
                overlay_input_handler handler(m_poll_input, m_set_intercepted, m_poll_touch);
                handler.is_paused = [this]() { return m_paused.load(std::memory_order_relaxed); };
                handler.is_abort_requested = [this]() { return m_input_thread_abort.load(); };
                result = input_context.target->run_input_loop(&handler, [this]() {
                    // Stop if interrupt status is set AND new items exist.
                    if (!m_input_thread_interrupted.load())
                        return true;
                    std::lock_guard lock(m_input_stack_guard);
                    return m_input_token_stack.empty();
                });
            }

            if (result == user_interface::selection_code::interrupted) {
                assert(m_input_thread_interrupted.load());
                interrupted_items.push_back(std::move(input_context));
                continue;
            }

            if (input_context.input_loop_epilogue) {
                input_context.input_loop_epilogue(result);
            } else if (result != 0 && result != user_interface::selection_code::canceled) {
                LOG_ERROR("Overlay \"{}\" exited with error code={}", input_context.name, result);
            }

            if (result == user_interface::selection_code::canceled) {
                break;
            }
        }

        if (!interrupted_items.empty()) {
            std::lock_guard lock(m_input_stack_guard);

            // Rebuild stack: interrupted items first (in insertion order), then new items.
            std::deque<input_thread_context_t> rebuilt;
            for (auto &item : interrupted_items) {
                rebuilt.push_back(std::move(item));
            }
            for (auto &item : m_input_token_stack) {
                rebuilt.push_back(std::move(item));
            }
            m_input_token_stack = std::move(rebuilt);
            interrupted_items.clear();
        } else if (!m_input_thread_abort) {
            // Wait for new items.
            std::unique_lock lock(m_input_stack_guard);
            m_input_stack_cv.wait(lock, [this] {
                return m_input_thread_abort.load() || !m_input_token_stack.empty();
            });
        }
    }
}

} // namespace overlay
