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

Callback::Callback(SceUID thread_id, const char *name, Ptr<SceKernelCallbackFunction> cb_func, Ptr<void> pCommon)
    : WithUidClass(name)
    , thread_id(thread_id)
    , cb_func(cb_func)
    , userdata(pCommon) {}

ThreadEventHandler::ThreadEventHandler(const char *name, SceUID target, SceUInt32 mask, Address handler, Address common)
    : WithUidClass(name)
    , target(target)
    , mask(mask)
    , handler(handler)
    , common(common) {}

bool ThreadEventHandler::runs_for(SceUID thread_id, SceUInt32 type) const {
    return !released && (mask & type) && (target == thread_id || target == SCE_KERNEL_THREAD_ID_PROCESS_ALL_ID);
}

bool ThreadEventHandler::claim_run() {
    const auto guard = lock();
    if (!guard || released)
        return false;
    ++runs;
    return true;
}

void ThreadEventHandler::end_run(std::optional<uint32_t> ret) {
    const auto guard = lock();
    if (!guard)
        return;
    --runs;
    if (ret && *ret != 0)
        released = true;
}

bool ThreadEventHandler::release() {
    const auto guard = lock();
    if (!guard || (released && runs == 0))
        return false;
    released = true;
    return true;
}
