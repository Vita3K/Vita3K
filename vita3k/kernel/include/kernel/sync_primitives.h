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
#include <span>
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
    const bool auto_reset;

    SimpleEvent(SceUInt32 attr, const char *name, SceUInt32 init_pattern);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceUInt32 init_pattern);

    SceInt32 wait(const ThreadStatePtr &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);
    SceInt32 set_or_pulse(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set);
    SceInt32 clear(SceUInt32 clear_pattern);
    SceInt32 cancel(SceUInt32 *num_wait_threads);
    // Sets result to the event's bit pattern, or returns UNKNOWN_EVENT_ID if it was deleted.
    SceInt32 get_pattern(SceUInt32 &result) const;

private:
    struct WaitEntry {
        SceUInt32 pattern;
        SceUInt32 *result_pattern;
        SceUInt64 *user_data;
    };

    void on_delete() override;
    // Takes the event if wait_pattern matches, or returns SCE_KERNEL_ERROR_EVENT_COND. The lock must be held.
    SceInt32 try_take(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);

    // Guarded by the lock
    WaitQueue<WaitEntry> waiters;
    SceUInt32 pattern;
    SceUInt64 last_user_data = 0;
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
    // Replaces the count and returns its old value, or the error.
    std::expected<uint64_t, SceInt32> set_time(uint64_t new_time);
    // Sets time to how much time the timer counted, or returns UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 get_time(uint64_t &time) const;
    // Sets time to the clock time the count runs from, 0 if the timer never started, or returns
    // UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 get_base(uint64_t &time) const;

private:
    void on_delete() override;
    void schedule_event();
    // Moves next_event to the first event after current_time, or to never if the event doesn't repeat.
    void advance_next_event(uint64_t current_time);
    // Takes the event if it is set by current_time, or returns SCE_KERNEL_ERROR_EVENT_COND. The lock must be held.
    SceInt32 try_take(SceUInt32 *result_pattern, SceUInt64 *user_data, uint64_t current_time);
    // Returns how much time the timer counted. The lock must be held.
    uint64_t count() const;

    // Guarded by the lock
    // Only the first waiter waits for the next event, the others wait for their turn
    WaitQueue<std::monostate> waiters;

    bool is_started = false;
    bool is_repeat = false;
    bool is_pulse = false;
    bool event_set = false;
    // The clock time the count runs from while the timer is started, 0 if it never started
    uint64_t base = 0;
    // The count while the timer is stopped
    uint64_t stopped_count = 0;
    uint64_t next_event = std::numeric_limits<uint64_t>::max();
    uint64_t event_interval = 0;
};

typedef std::shared_ptr<Timer> TimerPtr;

class Semaphore final : public WithUidClass<SyncPrimitive, UidClass::semaphore> {
public:
    const SceInt32 init_val;
    const SceInt32 max;

    Semaphore(SceUInt32 attr, const char *name, SceInt32 init_val, SceInt32 max_val);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceInt32 init_val, SceInt32 max_val);

    SceInt32 wait(const ThreadStatePtr &thread, SceInt32 need_count, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceInt32 need_count);
    SceInt32 signal(SceInt32 count);
    SceInt32 cancel(SceInt32 set_count, SceUInt32 *num_wait_threads);
    // Fills info with what the semaphore's info reports, or returns UNKNOWN_SEMA_ID if it was deleted.
    SceInt32 info(SceKernelSemaInfo &info) const;

private:
    struct WaitEntry {
        SceInt32 need_count;
    };

    void on_delete() override;

    // Guarded by the lock
    WaitQueue<WaitEntry> waiters;
    SceInt32 val;
};

typedef std::shared_ptr<Semaphore> SemaphorePtr;

class Mutex : public SyncPrimitive {
public:
    const SceInt32 init_count;
    const Ptr<SceKernelLwMutexWork> workarea;

    Mutex(SceUInt32 attr, const char *name, SceInt32 init_count, ThreadStatePtr thread, Ptr<SceKernelLwMutexWork> workarea = {});
    // Returns the error for a name, attributes or count the caller can't use, or 0. Kernel callers may
    // use attributes games can't.
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceInt32 init_count, bool kernel);

    SceInt32 acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count, SceUInt32 *timeout, bool callbacks);
    // Same as acquire, but reports the wait as target, for a condition variable taking its mutex back
    SceInt32 acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count, SceUInt32 *timeout, WaitTarget target, bool callbacks);
    SceInt32 try_acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count);
    SceInt32 release(MemState &mem, const ThreadStatePtr &thread, SceInt32 unlock_count);
    SceInt32 cancel(const ThreadStatePtr &thread, SceInt32 new_count, SceUInt32 *num_wait_threads);
    // Gives the mutex to its next waiter if thread, which is ending, owns it.
    void release_ended_owner(const ThreadState &thread);

protected:
    struct WaitEntry {
        SceInt32 lock_count;
    };

    // Guarded by the lock, read by the info of each kind of mutex
    WaitQueue<WaitEntry> waiters;
    SceInt32 lock_count;
    // The UID of the thread that holds it, or 0. A UID and not the thread, since a lightweight mutex
    // stays locked by an owner that ended and was deleted.
    SceUID owner;

private:
    void on_delete() override;
    bool lightweight() const { return get_uid_class() == UidClass::lw_mutex; }
    // Returns ILLEGAL_COUNT for a lock or unlock count the mutex can't take, or SCE_KERNEL_OK
    SceInt32 check_count(SceInt32 count) const;
    // Takes the mutex if it is free or owned by thread, or returns why it can't. The lock must be held.
    SceInt32 try_take(MemState &mem, const ThreadStatePtr &thread, SceInt32 count);
    // Gives the unlocked mutex to the first waiter, if there is one. The lock must be held.
    void hand_over();
    // Writes the owner and lock count to a lightweight mutex's work area, which the guest reads. The lock must be held.
    void update_workarea(MemState &mem);
};

class LwMutex final : public WithUidClass<Mutex, UidClass::lw_mutex> {
public:
    using WithUidClass::WithUidClass;

    // Fills info with what the lightweight mutex's info reports, or returns UNKNOWN_LW_MUTEX_ID if it
    // was deleted.
    SceInt32 info(SceKernelLwMutexInfo &info) const;
};

class HeavyMutex final : public WithUidClass<Mutex, UidClass::mutex> {
public:
    using WithUidClass::WithUidClass;

    // Fills info with what the mutex's info reports, or returns UNKNOWN_MUTEX_ID if it was deleted.
    SceInt32 info(SceKernelMutexInfo &info) const;
};

typedef std::shared_ptr<Mutex> MutexPtr;

class RWLock final : public WithUidClass<SyncPrimitive, UidClass::rw_lock> {
public:
    RWLock(SceUInt32 attr, const char *name);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr);

    SceInt32 acquire_read(const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 acquire_write(const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 try_acquire_read(const ThreadStatePtr &thread);
    SceInt32 try_acquire_write(const ThreadStatePtr &thread);
    SceInt32 release_read(const ThreadStatePtr &thread);
    SceInt32 release_write(const ThreadStatePtr &thread);
    SceInt32 cancel(const ThreadStatePtr &thread, SceUInt32 *num_read_wait_threads, SceUInt32 *num_write_wait_threads, SceInt32 flag);
    // Releases the write lock if thread, which is ending, holds it. A read lock stays held, like on
    // a Vita.
    void release_ended_writer(const ThreadState &thread);
    // Fills info with what the rwlock's info reports, or returns UNKNOWN_RW_LOCK_ID if it was deleted.
    SceInt32 info(SceKernelRWLockInfo &info) const;

private:
    struct WaitEntry {
        bool is_write;
    };

    void on_delete() override;
    SceInt32 acquire(const ThreadStatePtr &thread, bool is_write, SceUInt32 *timeout, bool callbacks);
    SceInt32 try_acquire(const ThreadStatePtr &thread, bool is_write);
    SceInt32 release(const ThreadStatePtr &thread, bool is_write);
    // Takes the lock if it is free for this kind of lock, or returns why it can't. The lock must be held.
    SceInt32 try_take(const ThreadStatePtr &thread, bool is_write);
    // Unlocks the lock nobody holds anymore, and gives it to the waiters that get it next. The lock
    // must be held.
    void hand_over();

    // Guarded by the lock
    // The read locks of every reader, or the writer's count. Readers aren't tracked one by one, like
    // on a Vita, so any thread may read unlock while readers hold it.
    SceInt32 lock_count = 0;
    // The UID of the thread that holds the write lock, or 0
    SceUID write_owner = 0;
    WaitQueue<WaitEntry> waiters;
};

typedef std::shared_ptr<RWLock> RWLockPtr;

class EventFlag final : public WithUidClass<SyncPrimitive, UidClass::event_flag> {
public:
    // The pattern the event flag was created with, for its info
    const SceUInt32 init_pattern;

    EventFlag(SceUInt32 attr, const char *name, SceUInt32 init_pattern);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr);
    // Returns the error a poll or wait for pattern with wait_mode gets, or SCE_KERNEL_OK
    static SceInt32 check_wait(SceUInt32 pattern, SceUInt32 wait_mode);

    SceInt32 wait(const ThreadStatePtr &thread, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits);
    SceInt32 set(SceUInt32 pattern);
    SceInt32 clear(SceUInt32 pattern);
    SceInt32 cancel(SceUInt32 pattern, SceUInt32 *num_wait_threads);
    // Fills info with what the event flag's info reports, or returns UNKNOWN_EVF_ID if it was deleted.
    SceInt32 info(SceKernelEventFlagInfo &info) const;

private:
    struct WaitEntry {
        SceUInt32 wait_mode;
        SceUInt32 pattern;
        SceUInt32 *out_bits;
    };

    void on_delete() override;
    // Takes the flags if pattern matches them, or returns why it can't. The lock must be held.
    SceInt32 try_take(SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits);

    // Guarded by the lock
    WaitQueue<WaitEntry> waiters;
    SceUInt32 flags;
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

    const MutexPtr associated_mutex;

    Condvar(SceUInt32 attr, const char *name, MutexPtr associated_mutex);

    SceInt32 wait(MemState &mem, const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 signal(SignalTarget target);
    // Returns how many threads wait on the condition variable, 0 once it was deleted.
    SceUInt32 num_waiters() const;

protected:
    // Guarded by the lock, read by the info of a condition variable
    WaitQueue<std::monostate> waiters;

private:
    void on_delete() override;
    bool lightweight() const { return get_uid_class() == UidClass::lw_cond; }
};

class LwCond final : public WithUidClass<Condvar, UidClass::lw_cond> {
public:
    LwCond(SceUInt32 attr, const char *name, MutexPtr associated_mutex, Ptr<SceKernelLwCondWork> workarea);

    // Fills info with what the lightweight condition variable's info reports, or returns
    // UNKNOWN_LW_COND_ID if it was deleted.
    SceInt32 info(SceKernelLwCondInfo &info) const;

    const Ptr<SceKernelLwCondWork> workarea;
};

class HeavyCond final : public WithUidClass<Condvar, UidClass::cond> {
public:
    using WithUidClass::WithUidClass;

    // Fills info with what the condition variable's info reports, or returns UNKNOWN_COND_ID if it was
    // deleted.
    SceInt32 info(SceKernelCondInfo &info) const;
};

typedef std::shared_ptr<Condvar> CondvarPtr;

// One piece of a message pipe transfer, a buffer in guest memory
struct MsgPipeVector {
    Ptr<void> base;
    SceSize size;
};

class MsgPipe final : public WithUidClass<SyncPrimitive, UidClass::msg_pipe> {
public:
    MsgPipe(SceUInt32 attr, const char *name, std::size_t buf_size);

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 type, SceUInt32 attr, SceSize buf_size);

    // Both move the bytes of the pieces in order, and return the size transferred, or the error
    std::expected<SceSize, SceInt32> receive(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceUInt32 *timeout, bool callbacks);
    std::expected<SceSize, SceInt32> send(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceUInt32 *timeout, bool callbacks);
    SceInt32 cancel(SceUInt32 *num_send_wait_threads, SceUInt32 *num_receive_wait_threads);
    // Fills info with what the message pipe's info reports, or returns UNKNOWN_MSG_PIPE_ID if it was
    // deleted.
    SceInt32 info(SceKernelMppInfo &info) const;

private:
    struct WaitEntry {
        SceSize request_size;
        // Woken to recheck the buffer and has not done so yet
        bool notified = false;
    };

    void on_delete() override;
    // Wakes the first waiter whose request now fits and that was not woken already, so it retries its transfer
    static void wake_waiter(WaitQueue<WaitEntry> &waiters, std::size_t available);

    // Guarded by the lock
    WaitQueue<WaitEntry> senders;
    WaitQueue<WaitEntry> receivers;
    ByteRingBuffer data_buffer;
};

typedef std::shared_ptr<MsgPipe> MsgPipePtr;
