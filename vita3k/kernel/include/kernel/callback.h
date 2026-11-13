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
#include <util/intrusive_list.h>

// A callback a thread created, which only that thread runs. Its notifications, runs and link in the
// owner's list are guarded by the owner's mutex, and the owner's callback calls change them.
class Callback final : public WithUidClass<KernelObject, UidClass::callback>, public IntrusiveLink<Callback> {
    friend class Ref<Callback>;

    Callback(SceUID thread_id, const char *name, Ptr<SceKernelCallbackFunction> cb_func, Ptr<void> pCommon);

public:
    // The notifications since the callback last ran, which its function receives
    struct Notification {
        // The last notifier's UID, SCE_UID_INVALID_UID for a direct notify, 0 with nothing pending
        SceUID notifier_id = 0;
        uint32_t count = 0;
        // The last notify's argument
        SceInt32 arg = 0;
    };

    // The thread that created this callback and runs it. Calls look it up by its UID, so they fail
    // once it's deleted.
    const SceUID thread_id;
    const Ptr<SceKernelCallbackFunction> cb_func; // Function to execute when the callback should run
    const Ptr<void> userdata; // User-provided data - passed as pCommon

private:
    friend struct ThreadState;

    // Guarded by the owner's mutex
    Notification pending;
    // The runs of the callback going on, more than one when it runs inside its own run
    uint32_t running_count = 0;
    // Deleted by a delete or a nonzero return. During a run the UID stays until the last run ends.
    bool deleted = false;
};

using CallbackRef = Ref<Callback>;

// A registered thread event handler, run on the threads of target for the event types in mask. Its
// deleted flag, its runs and its link in the kernel's list are guarded by the list's mutex.
class ThreadEventHandler final : public WithUidClass<KernelObject, UidClass::thread_event>, public IntrusiveLink<ThreadEventHandler> {
    friend class Ref<ThreadEventHandler>;

    ThreadEventHandler(const char *name, SceUID target, SceUInt32 mask, Address handler, Address common);

public:
    // Whether the handler runs on the thread for an event of type
    bool runs_for(SceUID thread_id, SceUInt32 type) const;

    // A thread UID, or SCE_KERNEL_THREAD_ID_PROCESS_ALL_ID for every thread of the process
    const SceUID target;
    const SceUInt32 mask;
    const Address handler;
    const Address common;

    // Set by an unregister or by a run that returned nonzero. No new run starts, and the handler
    // is unlisted once no run of it is left.
    bool deleted = false;
    // The runs of the handler going on, on any thread
    int runs = 0;
};

using ThreadEventHandlerRef = Ref<ThreadEventHandler>;
