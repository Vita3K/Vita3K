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

#include <kernel/callback.h>
#include <kernel/thread/thread_state.h>

#include <mutex>

Callback::Callback(SceUID uid, const ThreadStatePtr &owner, const std::string &name, Ptr<SceKernelCallbackFunction> cb_func, Ptr<void> pCommon)
    : thread_id(owner->id)
    , owner(owner)
    , name(name)
    , cb_func(cb_func)
    , userdata(pCommon) {
    this->uid = uid;
}

void Callback::notify(SceUID notifier_id, SceInt32 notify_arg) {
    {
        std::lock_guard lock(this->_mutex);
        if (this->deleted)
            return;
        this->notifier_id = notifier_id;
        this->notification_arg = notify_arg;
        this->num_notifications++;
    }
    if (const ThreadStatePtr thread = owner.lock())
        thread->notify_callbacks();
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

void Callback::mark_deleted() {
    std::lock_guard lock(this->_mutex);
    this->deleted = true;
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

uint32_t Callback::get_num_notifications() {
    std::lock_guard lock(this->_mutex);
    return this->num_notifications;
}

std::optional<Callback::Notification> Callback::take_notification() {
    std::lock_guard lock(this->_mutex);
    if (this->num_notifications == 0)
        return std::nullopt;

    const Notification notification{ this->notifier_id, this->num_notifications, this->notification_arg };
    this->reset();
    return notification;
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
