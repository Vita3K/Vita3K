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

struct ThreadEndWaiterRef {
    SceUID thread_id = SCE_UID_INVALID_UID;
    uint64_t wait_sequence = 0;
};

enum class WaitReason {
    None,
    Synchronization,
    Suspended,
};

enum class WaitType {
    None,
    Delay,
    VBlank,
    ThreadEnd,
    Signal,
    SimpleEvent,
    Timer,
    Mutex,
    RWLock,
    Semaphore,
    CondVar,
    EventFlag,
    MsgPipeSend,
    MsgPipeReceive,
};

enum class WaitEndReason {
    None,
    Signaled,
    Timeout,
    Canceled,
    Deleted,
};

struct WaitState {
    struct EventFlagData {
        uint32_t pattern = 0;
        uint32_t wait_mode = 0;
    };

    struct MsgPipeData {
        SceSize request_size = 0;
        uint32_t wait_mode = 0;
    };

    struct WaitWriteback {
        Address out_u32 = 0;
        Address out_u64 = 0;
        Address data_addr = 0;
    };

    struct WaitContinuation {
        WaitWriteback writeback;
        SceSize requested_size = 0;
        uint32_t flags = 0;
    };

    struct WaitCompletion {
        WaitEndReason end_reason = WaitEndReason::None;
        uint32_t value_u32 = 0;
        uint64_t value_u64 = 0;
        SceSize transferred_size = 0;
        bool condition_met = false;
    };

    WaitType type = WaitType::None;
    WaitEndReason end_reason = WaitEndReason::None;
    SceUID object_id = SCE_UID_INVALID_UID;
    uint64_t target_value = 0;
    uint32_t requested_timeout = 0;
    uint64_t timeout_deadline_us = 0;
    bool has_timeout = false;
    bool callbacks_allowed = false;
    union Payload {
        int32_t lock_count;
        bool is_write;
        int32_t need_count;
        uint32_t pattern;
        EventFlagData event_flag;
        MsgPipeData msgpipe;

        Payload()
            : pattern(0) {}
    } payload;
    WaitContinuation continuation;
    WaitCompletion completion;

    void clear() {
        *this = {};
    }

    static WaitState delay(const uint32_t delay_us, const bool callbacks_allowed = false) {
        WaitState state;
        state.type = WaitType::Delay;
        state.target_value = delay_us;
        state.callbacks_allowed = callbacks_allowed;
        return state;
    }

    static WaitState vblank(const uint64_t target_vcount, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::VBlank;
        state.target_value = target_vcount;
        state.callbacks_allowed = callbacks_allowed;
        return state;
    }

    static WaitState thread_end(const SceUID target_thread_id, const bool callbacks_allowed = false) {
        WaitState state;
        state.type = WaitType::ThreadEnd;
        state.object_id = target_thread_id;
        state.callbacks_allowed = callbacks_allowed;
        return state;
    }

    static WaitState signal(const uint32_t delay, const uint32_t timeout, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::Signal;
        state.target_value = delay;
        state.requested_timeout = timeout;
        state.has_timeout = timeout != 0;
        state.callbacks_allowed = callbacks_allowed;
        return state;
    }

    static WaitState simple_event(const SceUID event_id, const uint32_t pattern, const uint32_t timeout, const bool has_timeout, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::SimpleEvent;
        state.object_id = event_id;
        state.requested_timeout = timeout;
        state.has_timeout = has_timeout;
        state.callbacks_allowed = callbacks_allowed;
        state.payload.pattern = pattern;
        return state;
    }

    static WaitState timer(const SceUID timer_id, const uint32_t pattern, const uint32_t timeout, const bool has_timeout, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::Timer;
        state.object_id = timer_id;
        state.requested_timeout = timeout;
        state.has_timeout = has_timeout;
        state.callbacks_allowed = callbacks_allowed;
        state.payload.pattern = pattern;
        return state;
    }

    static WaitState mutex(const SceUID mutex_id, const int32_t lock_count, const uint32_t timeout, const bool has_timeout, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::Mutex;
        state.object_id = mutex_id;
        state.requested_timeout = timeout;
        state.has_timeout = has_timeout;
        state.callbacks_allowed = callbacks_allowed;
        state.payload.lock_count = lock_count;
        return state;
    }

    static WaitState rwlock(const SceUID lock_id, const bool is_write, const uint32_t timeout, const bool has_timeout, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::RWLock;
        state.object_id = lock_id;
        state.requested_timeout = timeout;
        state.has_timeout = has_timeout;
        state.callbacks_allowed = callbacks_allowed;
        state.payload.is_write = is_write;
        return state;
    }

    static WaitState semaphore(const SceUID semaphore_id, const int32_t need_count, const uint32_t timeout, const bool has_timeout, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::Semaphore;
        state.object_id = semaphore_id;
        state.requested_timeout = timeout;
        state.has_timeout = has_timeout;
        state.callbacks_allowed = callbacks_allowed;
        state.payload.need_count = need_count;
        return state;
    }

    static WaitState condvar(const SceUID condvar_id, const uint32_t timeout, const bool has_timeout, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::CondVar;
        state.object_id = condvar_id;
        state.requested_timeout = timeout;
        state.has_timeout = has_timeout;
        state.callbacks_allowed = callbacks_allowed;
        return state;
    }

    static WaitState event_flag(const SceUID event_flag_id, const uint32_t pattern, const uint32_t wait_mode, const uint32_t timeout, const bool has_timeout, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::EventFlag;
        state.object_id = event_flag_id;
        state.requested_timeout = timeout;
        state.has_timeout = has_timeout;
        state.callbacks_allowed = callbacks_allowed;
        state.payload.event_flag.pattern = pattern;
        state.payload.event_flag.wait_mode = wait_mode;
        return state;
    }

    static WaitState msgpipe_send(const SceUID msgpipe_id, const SceSize request_size, const uint32_t wait_mode, const uint32_t timeout, const bool has_timeout, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::MsgPipeSend;
        state.object_id = msgpipe_id;
        state.requested_timeout = timeout;
        state.has_timeout = has_timeout;
        state.callbacks_allowed = callbacks_allowed;
        state.payload.msgpipe.request_size = request_size;
        state.payload.msgpipe.wait_mode = wait_mode;
        return state;
    }

    static WaitState msgpipe_receive(const SceUID msgpipe_id, const SceSize request_size, const uint32_t wait_mode, const uint32_t timeout, const bool has_timeout, const bool callbacks_allowed) {
        WaitState state;
        state.type = WaitType::MsgPipeReceive;
        state.object_id = msgpipe_id;
        state.requested_timeout = timeout;
        state.has_timeout = has_timeout;
        state.callbacks_allowed = callbacks_allowed;
        state.payload.msgpipe.request_size = request_size;
        state.payload.msgpipe.wait_mode = wait_mode;
        return state;
    }
};

struct InterruptedWaitState {
    WaitState wait;
    WaitReason wait_reason = WaitReason::None;
    uint64_t sequence = 0;
    bool callback_wakeup_pending = false;
    bool valid = false;

    void clear() {
        *this = {};
    }
};
