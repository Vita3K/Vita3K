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
#include <kernel/thread/thread_state.h>
#include <kernel/thread/wait_queue.h>
#include <kernel/types.h>
#include <util/byte_ring_buffer.h>

#include <expected>
#include <limits>
#include <map>
#include <string>
#include <variant>

class SyncPrimitive : public KernelObject {
public:
    // Returns the error for a name the guest can't use, or 0.
    static SceInt32 check_name(const char *name, SceUInt32 attr);

    const SceUInt32 attr;
    const std::string name;

protected:
    SyncPrimitive(SceUInt32 attr, const char *name);
};

class SimpleEvent final : public WithUidClass<SyncPrimitive, UidClass::simple_event> {
public:
    SimpleEvent(SceUInt32 attr, const char *name, SceUInt32 init_pattern);

    SceInt32 wait(const ThreadStatePtr &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);
    SceInt32 set_or_pulse(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set);
    SceInt32 clear(SceUInt32 clear_pattern);
    SceInt32 cancel(SceUInt32 *num_wait_threads);

    struct WaitEntry {
        SceUInt32 pattern;
        SceUInt32 *result_pattern;
        SceUInt64 *user_data;
    };

    const bool auto_reset;
    WaitQueue<WaitEntry> waiters;
    SceUInt32 pattern;
    SceUInt64 last_user_data = 0;

private:
    void on_delete() override;
    // Takes the event if wait_pattern matches, or returns SCE_KERNEL_ERROR_EVENT_COND. The lock must be held.
    SceInt32 try_take(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);
};

typedef std::shared_ptr<SimpleEvent> SimpleEventPtr;

class Timer final : public WithUidClass<SyncPrimitive, UidClass::timer> {
public:
    Timer(SceUInt32 attr, const char *name);

    SceInt32 set_event(SceUID type, SceKernelSysClock interval, SceInt32 repeats);
    SceInt32 wait(const ThreadStatePtr &thread, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceUInt32 *result_pattern, SceUInt64 *user_data);
    SceInt32 clear();
    SceInt32 start();
    SceInt32 stop();
    SceInt32 cancel(SceUInt32 *num_wait_threads);
    // Replaces time and returns its old value, or the error.
    std::expected<uint64_t, SceInt32> set_time(uint64_t new_time);

    // Only the first waiter waits for the next event, the others wait for their turn
    WaitQueue<std::monostate> waiters;

    bool is_started = false;
    bool is_repeat = false;
    bool is_pulse = false;
    bool event_set = false;
    uint64_t time = 0;
    uint64_t next_event = std::numeric_limits<uint64_t>::max();
    uint64_t event_interval = 0;

private:
    void on_delete() override;
    void schedule_event();
    // Moves next_event to the first event after current_time, or to never if the event doesn't repeat.
    void advance_next_event(uint64_t current_time);
    // Takes the event if it is set by current_time, or returns SCE_KERNEL_ERROR_EVENT_COND. The lock must be held.
    SceInt32 try_take(SceUInt32 *result_pattern, SceUInt64 *user_data, uint64_t current_time);
};

typedef std::shared_ptr<Timer> TimerPtr;

class Semaphore final : public WithUidClass<SyncPrimitive, UidClass::semaphore> {
public:
    Semaphore(SceUInt32 attr, const char *name, int init_val, int max_val);

    SceInt32 wait(const ThreadStatePtr &thread, SceInt32 need_count, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceInt32 need_count);
    SceInt32 signal(SceInt32 count);
    SceInt32 cancel(SceInt32 set_count, SceUInt32 *num_wait_threads);

    struct WaitEntry {
        int32_t need_count;
    };

    const int init_val;
    const int max;
    WaitQueue<WaitEntry> waiters;
    int val;

private:
    void on_delete() override;
};

typedef std::shared_ptr<Semaphore> SemaphorePtr;

class Mutex : public SyncPrimitive {
public:
    Mutex(SceUInt32 attr, const char *name, int init_count, ThreadStatePtr thread, Ptr<SceKernelLwMutexWork> workarea = {});
    // Returns the error for a name or count the guest can't use, or 0.
    static SceInt32 check_create(const char *name, SceUInt32 attr, int init_count);

    SceInt32 acquire(MemState &mem, const ThreadStatePtr &thread, int count, SceUInt32 *timeout, bool callbacks);
    // Same as acquire, but reports the wait as target, for a condition variable taking its mutex back
    SceInt32 acquire(MemState &mem, const ThreadStatePtr &thread, int count, SceUInt32 *timeout, WaitTarget target, bool callbacks);
    SceInt32 try_acquire(MemState &mem, const ThreadStatePtr &thread, int count);
    SceInt32 release(const ThreadStatePtr &thread, int unlock_count);
    SceInt32 cancel(const ThreadStatePtr &thread, int new_count, SceUInt32 *num_wait_threads);

    struct WaitEntry {
        int32_t lock_count;
    };

    const int init_count;
    const Ptr<SceKernelLwMutexWork> workarea;
    WaitQueue<WaitEntry> waiters;
    int lock_count;
    ThreadStatePtr owner;

private:
    void on_delete() override;
    bool lightweight() const { return get_uid_class() == UidClass::lw_mutex; }
    // Takes the mutex if it is free or owned by thread, or returns why it can't. The lock must be held.
    SceInt32 try_take(MemState &mem, const ThreadStatePtr &thread, int count);
};

class LwMutex final : public WithUidClass<Mutex, UidClass::lw_mutex> {
public:
    using WithUidClass::WithUidClass;
};

class HeavyMutex final : public WithUidClass<Mutex, UidClass::mutex> {
public:
    using WithUidClass::WithUidClass;
};

typedef std::shared_ptr<Mutex> MutexPtr;

enum class RWLockState {
    Unlocked,
    ReadLocked,
    WriteLocked,
};

class RWLock final : public WithUidClass<SyncPrimitive, UidClass::rw_lock> {
public:
    RWLock(SceUInt32 attr, const char *name);

    SceInt32 acquire_read(const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 acquire_write(const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 release(const ThreadStatePtr &thread);
    SceInt32 cancel(const ThreadStatePtr &thread, SceUInt32 *num_read_wait_threads, SceUInt32 *num_write_wait_threads, SceInt32 flag);

    struct WaitEntry {
        bool is_write;
    };

    RWLockState state = RWLockState::Unlocked;
    // the int value is the lock count for recursive locks
    std::map<ThreadStatePtr, int> owners;
    WaitQueue<WaitEntry> waiters;

private:
    void on_delete() override;
    SceInt32 acquire(const ThreadStatePtr &thread, bool is_write, SceUInt32 *timeout, bool callbacks);
};

typedef std::shared_ptr<RWLock> RWLockPtr;

class EventFlag final : public WithUidClass<SyncPrimitive, UidClass::event_flag> {
public:
    EventFlag(SceUInt32 attr, const char *name, SceUInt32 init_pattern);

    SceInt32 wait(const ThreadStatePtr &thread, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits);
    SceInt32 set(SceUInt32 pattern);
    SceInt32 clear(SceUInt32 pattern);
    SceInt32 cancel(SceUInt32 pattern, SceUInt32 *num_wait_threads);

    struct WaitEntry {
        SceUInt32 wait_mode;
        SceUInt32 pattern;
        SceUInt32 *out_bits;
    };

    WaitQueue<WaitEntry> waiters;
    SceUInt32 flags;

private:
    void on_delete() override;
    // Takes the flags if pattern matches them, or returns why it can't. The lock must be held.
    SceInt32 try_take(SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits);
};

typedef std::shared_ptr<EventFlag> EventFlagPtr;

class Condvar : public SyncPrimitive {
public:
    struct SignalTarget {
        enum class Type {
            Any, // signal any one waiting thread
            Specific, // signal a specific waiting thread (target_thread)
            All, // signal all waiting threads
        } type;

        SceUID thread_id; // for Type::One

        explicit SignalTarget(Type type)
            : type(type)
            , thread_id(0) {}
        SignalTarget(Type type, SceUID thread_id)
            : type(type)
            , thread_id(thread_id) {}
    };

    Condvar(SceUInt32 attr, const char *name, MutexPtr associated_mutex);

    SceInt32 wait(MemState &mem, const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 signal(SignalTarget target);

    const MutexPtr associated_mutex;
    WaitQueue<std::monostate> waiters;

private:
    void on_delete() override;
    bool lightweight() const { return get_uid_class() == UidClass::lw_cond; }
};

class LwCond final : public WithUidClass<Condvar, UidClass::lw_cond> {
public:
    using WithUidClass::WithUidClass;
};

class HeavyCond final : public WithUidClass<Condvar, UidClass::cond> {
public:
    using WithUidClass::WithUidClass;
};

typedef std::shared_ptr<Condvar> CondvarPtr;

class MsgPipe final : public WithUidClass<SyncPrimitive, UidClass::msg_pipe> {
public:
    MsgPipe(SceUInt32 attr, const char *name, std::size_t buf_size);

    // Both return the size transferred, or the error
    std::expected<SceSize, SceInt32> receive(const ThreadStatePtr &thread, SceUInt32 wait_mode, void *buf, SceSize size, SceUInt32 *timeout, bool callbacks);
    std::expected<SceSize, SceInt32> send(const ThreadStatePtr &thread, SceUInt32 wait_mode, const void *buf, SceSize size, SceUInt32 *timeout, bool callbacks);
    SceInt32 cancel(SceUInt32 *num_send_wait_threads, SceUInt32 *num_receive_wait_threads);

private:
    struct WaitEntry {
        SceSize request_size;
        // Woken to recheck the buffer and has not done so yet
        bool notified = false;
    };

    void on_delete() override;
    // Wakes the first waiter whose request now fits and that was not woken already, so it retries its transfer
    static void wake_waiter(WaitQueue<WaitEntry> &waiters, std::size_t available);

    // TODO do senders respect priority?
    WaitQueue<WaitEntry> senders;
    WaitQueue<WaitEntry> receivers;
    ByteRingBuffer data_buffer;
};

typedef std::shared_ptr<MsgPipe> MsgPipePtr;
