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

#include <kernel/kernel_object.h>
#include <kernel/types.h>
#include <memory>
#include <optional>
#include <string>

struct ThreadState;
typedef std::shared_ptr<ThreadState> ThreadStatePtr;

class Callback final : public WithUidClass<KernelObject, UidClass::callback> {
public:
    // What the callback function receives about the notifications since it last ran
    struct Notification {
        SceUID notifier_id;
        uint32_t count;
        SceInt32 arg;
    };

    Callback(const ThreadStatePtr &owner, const std::string &name, Ptr<SceKernelCallbackFunction> cb_func, Ptr<void> pCommon);

    void notify(SceUID notifier_id, SceInt32 notify_arg);
    // Notifies from an event, without notification argument
    void event_notify(SceUID notifier_id);
    // Notifies directly, not from an event
    void direct_notify(SceInt32 notify_arg);
    // Cancels every notification sent to this callback
    void cancel();

    // UID of the event that notified the callback last
    SceUID get_notifier_id();
    // notifyArg from the last time the callback was notified
    SceInt32 get_notify_arg();
    // Number of times the callback has been notified since it last ran
    uint32_t get_num_notifications();

    // Takes the pending notifications, so new ones are kept for the next run. Returns nothing if the callback was not notified.
    std::optional<Notification> take_notification();

    const SceUID thread_id; // UID of the thread that created this callback
    const std::string name;
    const Ptr<SceKernelCallbackFunction> cb_func; // Function to execute when the callback should run
    const Ptr<void> userdata; // User-provided data - passed as pCommon

private:
    // Drops the pending notifications, so the callback never runs again
    void on_delete() override;
    // Clears the notifications. The lock must be held.
    void reset();

    const std::weak_ptr<ThreadState> owner; // Thread that created this callback, woken when it is notified

    uint32_t num_notifications = 0; // Number of times this callback has been notified - reset every time it is run
    SceInt32 notification_arg = 0; // User-specified argument passed by sceKernelNotifyCallback
    SceUID notifier_id = SCE_UID_INVALID_UID; // UID of the last event that notified this thread - SCE_UID_INVALID_UID if not an event
};

typedef std::shared_ptr<Callback> CallbackPtr;
