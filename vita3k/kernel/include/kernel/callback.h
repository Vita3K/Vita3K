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
#include <string>

// A callback a thread created, which only that thread runs. Its notifications are guarded by the
// owner's mutex, and the owner's callback calls change them.
class Callback final : public WithUidClass<KernelObject, UidClass::callback> {
public:
    // The notifications since the callback last ran, which its function receives
    struct Notification {
        // The last notifier's UID, SCE_UID_INVALID_UID for a direct notify
        SceUID notifier_id = SCE_UID_INVALID_UID;
        uint32_t count = 0;
        // The last notify's argument
        SceInt32 arg = 0;
    };

    Callback(SceUID thread_id, const std::string &name, Ptr<SceKernelCallbackFunction> cb_func, Ptr<void> pCommon);

    // The thread that created this callback and runs it. Calls look it up by its UID, so they fail
    // once it's deleted.
    const SceUID thread_id;
    const std::string name;
    const Ptr<SceKernelCallbackFunction> cb_func; // Function to execute when the callback should run
    const Ptr<void> userdata; // User-provided data - passed as pCommon

private:
    friend struct ThreadState;

    // Guarded by the owner's mutex
    Notification pending;
    // Deleted, so it takes no more notifies
    bool deleted = false;
};

using CallbackPtr = std::shared_ptr<Callback>;
