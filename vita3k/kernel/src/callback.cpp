// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <kernel/callback.h>
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>
#include <mutex>
#include <util/log.h>

uint32_t process_callbacks(KernelState &kernel, SceUID thread_id) {
    ThreadStatePtr thread = kernel.get_thread(thread_id);
    if (thread->is_processing_callbacks)
        return 0;

    thread->is_processing_callbacks = true;
    uint32_t num_callbacks_processed = 0;
    for (CallbackPtr &cb : thread->callbacks) {
        if (cb->is_executable()) {
            std::string name = cb->get_name();
            cb->execute(kernel, [name]() {
                LOG_WARN("Callback with name {} requested to be deleted, but this is not supported yet!", name);
            });
            num_callbacks_processed++;
        }
    }
    thread->is_processing_callbacks = false;

    return num_callbacks_processed;
}

void Callback::notify(SceUID notifier_id, SceInt32 notify_arg) {
    std::lock_guard lock(this->_mutex);
    this->notifier_id = notifier_id;
    this->notification_arg = notify_arg;
    this->num_notifications++;
}

void Callback::event_notify(SceUID notifier_id) {
    this->notify(notifier_id, 0);
}

void Callback::direct_notify(SceInt32 notify_arg) {
    this->notify(SCE_UID_INVALID_UID, notify_arg);
}

void Callback::cancel() {
    std::lock_guard lock(this->_mutex);
    this->reset();
}

SceUID Callback::get_notifier_id() {
    std::lock_guard lock(this->_mutex);
    return this->notifier_id;
}

SceInt32 Callback::get_notify_arg() {
    std::lock_guard lock(this->_mutex);
    return this->notification_arg;
}

bool Callback::is_executable() {
    // Lock the mutex to ensure that no other call is accessing this Callback
    // If we don't lock, num_notifications could be modified right as we're reading it
    std::lock_guard lock(this->_mutex);
    return this->is_notified();
}

uint32_t Callback::get_num_notifications() {
    std::lock_guard lock(this->_mutex);
    return this->num_notifications;
}

void Callback::execute(KernelState &kernel, const std::function<void()> &deleter) {
    std::lock_guard lock(this->_mutex);
    if (!this->is_notified())
        return;

    std::vector<uint32_t> args = { (uint32_t)(this->notifier_id), this->num_notifications, (uint32_t)this->notification_arg, this->userdata.address() };
    int ret = kernel.get_thread(this->thread_id)->run_callback(this->cb_func.address(), args);
    if (ret != 0) {
        deleter();
    }
    this->reset(); // Callbacks return to their default state after running
}

/** Private methods **/

/**
 * @brief Resets the callback to its default state
 * @note You MUST lock the callback's mutex before calling this function
 */
void Callback::reset() {
    this->num_notifications = 0;
    this->notifier_id = SCE_UID_INVALID_UID;
}

/**
 * @return true if the callback has notifications pending, false otherwise
 * @note You MUST lock the callback's mutex before calling this function
 */
bool Callback::is_notified() const {
    return (this->num_notifications > 0);
}
