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
#include <kernel/types.h>
#include <kernel/waitable.h>
#include <util/byte_ring_buffer.h>
#include <util/intrusive_list.h>

#include <condition_variable>
#include <expected>
#include <limits>
#include <span>
#include <thread>

// The ID of the one process Vita3K runs
constexpr SceUID PROCESS_ID = 1;

// Returns the user data of a system event a thread caused, like a delete: its UID in the high word
// and the process ID in the low word.
constexpr SceUInt64 system_event_user_data(SceUID thread_id) {
    return (static_cast<SceUInt64>(thread_id) << 32) | static_cast<SceUInt32>(PROCESS_ID);
}

// A callback registered to an event, with the notify ID its function gets
struct CallbackRegistration : IntrusiveLink<CallbackRegistration> {
    CallbackRegistration(KernelState &kernel, CallbackRef callback, SceUID notify_id)
        : kernel(kernel)
        , callback(std::move(callback))
        , notify_id(notify_id) {}

    // The kernel that finds the callback's owner thread
    KernelState &kernel;
    const CallbackRef callback;
    const SceUID notify_id;
};

class TimerService;

// The event part of simple events and timers: a bit pattern, the user data of the last set, and the
// waits for bits of the pattern.
class Event : public Waitable {
public:
    const bool auto_reset;

    SceInt32 wait(ThreadState &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks);

    SceInt32 poll(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);
    SceInt32 clear(SceUInt32 clear_pattern);
    virtual SceInt32 cancel(SceUInt32 *num_wait_threads);
    // Cancels the waits like cancel(), then replaces the whole pattern and the user data.
    SceInt32 cancel(SceUInt32 set_pattern, SceUInt64 user_data, SceUInt32 *num_wait_threads);
    // Sets result to the event's bit pattern, or returns UNKNOWN_EVENT_ID if it was deleted.
    SceInt32 get_pattern(SceUInt32 &result);
    // Fills info with what the event's info reports, or returns UNKNOWN_EVENT_ID if it was deleted.
    SceInt32 info(SceKernelEventInfo &info);
    // Registers callback, which the event's notifications then reach with notify_id. One callback can
    // be registered more than once.
    SceInt32 register_callback(KernelState &kernel, CallbackRef callback, SceUID notify_id);
    // Removes the first registration of the callback
    SceInt32 unregister_callback(SceUID callback_id);
    // Removes every registration
    SceInt32 unregister_callbacks();

protected:
    Event(SceUInt32 attr, const char *name, SceUInt32 init_pattern, SceUInt64 init_user_data);

    void end_waits(SceUID deleter) override;
    // Takes the event if wait_pattern matches, or returns SCE_KERNEL_ERROR_EVENT_COND. The lock must be held.
    SceInt32 try_take(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);
    // Returns the guest's result of a wait, and gives a wait that a delete or a cancel ended the error
    // event.
    SceInt32 wait_result(SceInt32 r, SceUInt32 *result_pattern);
    // Sets or pulses the bits with user_data and gives them to the waits they match. The lock must be
    // held.
    void fire(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set);
    // Gives the event's bits to the waits they match.
    void serve() override;
    // Notifies the registered callbacks with arg and drops the ones that are gone. The lock must be
    // held.
    void notify_callbacks(SceInt32 arg);

    // Guarded by the lock
    SceUInt32 pattern;
    SceUInt64 last_user_data;
    // In callback UID order
    UniqueList<CallbackRegistration> registrations;
};

class SimpleEvent final : public WithUidClass<Event, UidClass::simple_event> {
    friend class Ref<SimpleEvent>;

    SimpleEvent(SceUInt32 attr, const char *name, SceUInt32 init_pattern, SceUID creator);

public:
    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceUInt32 init_pattern);

    SceInt32 set_or_pulse(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set);
    // Sets or pulses the bits, then notifies the registered callbacks with notify_arg
    SceInt32 set_or_pulse_and_notify(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set, SceInt32 notify_arg);
    // Pulses bits of an event the kernel made, which aren't limited to the user defined ones, and
    // notifies the registered callbacks with notify_arg
    void pulse_and_notify(SceUInt32 pattern, SceInt32 notify_arg);
    // Sets one of the system events, like the open event, with user_data for its waiters, or returns
    // UNKNOWN_EVENT_ID if it was deleted.
    SceInt32 set_system_event(SceUInt32 event, SceUInt64 user_data);
};

using SimpleEventRef = Ref<SimpleEvent>;

using EventRef = Ref<Event>;

class Timer final : public WithUidClass<Event, UidClass::timer>, public IntrusiveLink<Timer> {
    friend class Ref<Timer>;
    friend class TimerService;

    Timer(TimerService &timers, SceUInt32 attr, const char *name, SceUID creator);

public:
    SceInt32 set_event(SceUID type, SceKernelSysClock interval, SceInt32 repeats);
    SceInt32 start();
    SceInt32 stop();
    SceInt32 cancel(SceUInt32 *num_wait_threads) override;
    // Replaces the count and returns its old value, or the error.
    std::expected<uint64_t, SceInt32> set_time(uint64_t new_time);
    // Sets time to how much time the timer counted, or returns UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 get_time(uint64_t &time) const;
    // Sets time to the clock time the count runs from, 0 while the timer is stopped, or returns
    // UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 get_base(uint64_t &time) const;
    // Sets time to how much time the armed event has left, or returns EVENT_NOT_SET with no event
    // armed or UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 remaining_time(uint64_t &time);
    // Fills info with what the timer's info reports, with the clock time the count runs from as the
    // base of a running timer, or returns UNKNOWN_TIMER_ID if it was deleted.
    SceInt32 timer_info(SceKernelTimerInfo &info);

private:
    // Fires an event still due by cutoff, then rearms a repeating event.
    void expire(uint64_t cutoff);
    void end_waits(SceUID deleter) override;
    // Returns how much time the timer counted. The lock must be held.
    uint64_t count() const;

    TimerService &timers;
    // Written with both locks held, read with either.
    uint64_t deadline = 0;

    // Guarded by the lock
    bool is_started = false;
    // An event is set, and a fired one shot event stays armed
    bool armed = false;
    bool is_repeat = false;
    bool is_pulse = false;
    // The clock time the count runs from while the timer is started, 0 if it never started
    uint64_t base = 0;
    // The count while the timer is stopped
    uint64_t stopped_count = 0;
    // The count the armed event is due at
    uint64_t schedule = 0;
    uint64_t event_interval = 0;
};

using TimerRef = Ref<Timer>;

// Armed timers in deadline order, fired by one host thread.
class TimerService {
public:
    ~TimerService();
    void start();
    void stop();

    // Called with the timer lock held. The service lock comes after it.
    void arm(Timer &timer, uint64_t deadline);
    void disarm(Timer &timer);
    bool take_due(Timer &timer, uint64_t cutoff);

private:
    void run();
    // Takes timer off the list if it is on it, and moves the batch on if it was next
    void unlink(Timer &timer);

    std::mutex mutex;
    std::condition_variable wake;
    RefList<Timer> timers;
    // The next timer of the batch being fired, guarded by the lock
    IntrusiveList<Timer>::iterator cursor = timers.end();
    bool stopping = false;
    std::thread thread;
};

class Semaphore final : public WithUidClass<Waitable, UidClass::semaphore> {
    friend class Ref<Semaphore>;

    Semaphore(SceUInt32 attr, const char *name, SceInt32 init_val, SceInt32 max_val);

public:
    const SceInt32 init_val;
    const SceInt32 max;

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceInt32 init_val, SceInt32 max_val);

    SceInt32 wait(ThreadState &thread, SceInt32 need_count, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(ThreadState &thread, SceInt32 need_count);
    SceInt32 signal(SceInt32 count);
    SceInt32 cancel(SceInt32 set_count, SceUInt32 *num_wait_threads);
    // Fills info with what the semaphore's info reports, or returns UNKNOWN_SEMA_ID if it was deleted.
    SceInt32 info(SceKernelSemaInfo &info) const;

private:
    void end_waits(SceUID deleter) override;
    // Takes need_count if the count covers it and no waiter is queued ahead of thread. The lock
    // must be held.
    bool try_take(ThreadState &thread, SceInt32 need_count);
    // Gives the count to the waits it covers, in queue order up to the first it doesn't.
    void serve() override;

    // Guarded by the lock
    SceInt32 val;
};

using SemaphoreRef = Ref<Semaphore>;

class Mutex : public Waitable, public IntrusiveLink<Mutex> {
public:
    const SceInt32 init_count;
    const Ptr<SceKernelLwMutexWork> workarea;
    // The ceiling priority a heavy mutex was created with, SCE_KERNEL_LOWEST_PRIORITY_KERNEL for none.
    // It's reported, not applied yet.
    const int ceiling;

    // Returns the error for arguments the caller can't use, or 0. Kernel callers may use more attributes.
    static SceInt32 check_create(const char *name, SceUInt32 attr, SceInt32 init_count, const SceKernelMutexOptParam *opt, bool kernel);

    SceInt32 acquire(MemState &mem, ThreadState &thread, SceInt32 count, SceUInt32 *timeout, bool callbacks);
    // Relocks the mutex after a condition variable wait, without a timeout.
    SceInt32 relock(MemState &mem, ThreadState &thread, bool callbacks);
    SceInt32 try_acquire(MemState &mem, ThreadState &thread, SceInt32 count);
    SceInt32 release(MemState &mem, ThreadState &thread, SceInt32 unlock_count);
    SceInt32 cancel(ThreadState &thread, SceInt32 new_count, SceUInt32 *num_wait_threads);
    // Gives a heavy mutex to its next waiter if thread, which is ending, still owns it.
    void release_if_owned_by(ThreadState &thread);

protected:
    Mutex(SceUInt32 attr, const char *name, SceInt32 init_count, ThreadStateRef thread, int ceiling, Ptr<SceKernelLwMutexWork> workarea = {});

    // Makes thread the owner, and lists a heavy mutex in what it holds. The lock must be held, except
    // while the mutex is created.
    void set_owner(ThreadState &thread);
    // Leaves the mutex with no owner and unlists it. The lock must be held.
    void clear_owner();

    // Guarded by the lock, read by the info of each kind of mutex
    SceInt32 lock_count;
    // The UID of the thread that holds it, or 0. A UID and not the thread, since a lightweight mutex
    // stays locked by an owner that ended and was deleted.
    SceUID owner;

private:
    // The owner of a heavy mutex, which lists it, or null
    ThreadState *owner_thread = nullptr;

    void end_waits(SceUID deleter) override;
    bool lightweight() const { return get_uid_class() == UidClass::lw_mutex; }
    // Returns ILLEGAL_COUNT for a lock or unlock count the mutex can't take, or SCE_KERNEL_OK
    SceInt32 check_count(SceInt32 count) const;
    // Takes the mutex if it is free or owned by thread, or returns why it can't. The lock must be held.
    SceInt32 try_take(MemState &mem, ThreadState &thread, SceInt32 count);
    // Gives a free mutex to the first waiter, if there is one.
    void serve() override;
    // Writes the owner and lock count to a lightweight mutex's work area, which the guest reads. The lock must be held.
    void update_workarea(MemState &mem);
};

class LwMutex final : public WithUidClass<Mutex, UidClass::lw_mutex> {
    friend class Ref<LwMutex>;

    using WithUidClass::WithUidClass;

public:
    // Fills info with what the lightweight mutex's info reports, or returns UNKNOWN_LW_MUTEX_ID if it
    // was deleted.
    SceInt32 info(SceKernelLwMutexInfo &info) const;
};

class HeavyMutex final : public WithUidClass<Mutex, UidClass::mutex> {
    friend class Ref<HeavyMutex>;

    HeavyMutex(SceUInt32 attr, const char *name, SceInt32 init_count, ThreadStateRef thread, int ceiling);

public:
    // Fills info with what the mutex's info reports, or returns UNKNOWN_MUTEX_ID if it was deleted.
    SceInt32 info(SceKernelMutexInfo &info) const;
};

using MutexRef = Ref<Mutex>;

class RWLock final : public WithUidClass<Waitable, UidClass::rw_lock>, public IntrusiveLink<RWLock> {
    friend class Ref<RWLock>;

    RWLock(SceUInt32 attr, const char *name);

public:
    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr);

    SceInt32 acquire_read(ThreadState &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 acquire_write(ThreadState &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 try_acquire_read(ThreadState &thread);
    SceInt32 try_acquire_write(ThreadState &thread);
    SceInt32 release_read(ThreadState &thread);
    SceInt32 release_write(ThreadState &thread);
    SceInt32 cancel(ThreadState &thread, SceUInt32 *num_read_wait_threads, SceUInt32 *num_write_wait_threads, SceInt32 flag);
    // Releases the write lock if thread, which is ending, still holds it. A read lock stays held, like on
    // a Vita.
    void release_if_write_locked_by(ThreadState &thread);
    // Fills info with what the rwlock's info reports, or returns UNKNOWN_RW_LOCK_ID if it was deleted.
    SceInt32 info(SceKernelRWLockInfo &info) const;

private:
    void end_waits(SceUID deleter) override;
    SceInt32 acquire(ThreadState &thread, bool is_write, SceUInt32 *timeout, bool callbacks);
    SceInt32 try_acquire(ThreadState &thread, bool is_write);
    SceInt32 release(ThreadState &thread, bool is_write);
    // Takes the lock if it is free for this kind of lock, or returns why it can't. The lock must be held.
    SceInt32 try_take(ThreadState &thread, bool is_write);
    // Makes thread the writer and lists the lock in what it holds. The lock must be held.
    void set_write_owner(ThreadState &thread);
    // Leaves the lock with no writer and unlists it. The lock must be held.
    void clear_write_owner();
    // Gives the lock to the waiters that get it next: every waiting reader while no writer holds or
    // waits for it, or else the first waiting writer once nobody holds it.
    void serve() override;

    // Guarded by the lock
    // The read locks of every reader, or the writer's count. Readers aren't tracked one by one, like
    // on a Vita, so any thread may read unlock while readers hold it.
    SceInt32 lock_count = 0;
    // The thread that holds the write lock, or null
    ThreadState *write_owner = nullptr;
};

using RWLockRef = Ref<RWLock>;

class EventFlag final : public WithUidClass<Waitable, UidClass::event_flag>, public IntrusiveLink<EventFlag> {
    friend class Ref<EventFlag>;

    EventFlag(SceUInt32 attr, const char *name, SceUInt32 init_pattern);

public:
    // The pattern the event flag was created with, for its info
    const SceUInt32 init_pattern;

    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 attr);
    // Returns the error a poll or wait for pattern with wait_mode gets, or SCE_KERNEL_OK
    static SceInt32 check_wait(SceUInt32 pattern, SceUInt32 wait_mode);

    SceInt32 wait(ThreadState &thread, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits, SceUInt32 *timeout, bool callbacks);
    SceInt32 poll(SceUID thread_id, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits);
    SceInt32 set(SceUInt32 pattern);
    SceInt32 clear(SceUInt32 pattern);
    SceInt32 cancel(SceUInt32 pattern, SceUInt32 *num_wait_threads);
    // Fills info with what the event flag's info reports, or returns UNKNOWN_EVF_ID if it was deleted.
    SceInt32 info(SceKernelEventFlagInfo &info) const;
    // Drops the holds on the flag if thread, which is ending, still has them.
    void release_if_held_by(ThreadState &thread);

private:
    void end_waits(SceUID deleter) override;
    // Returns whether another thread's wait holds the flag. The lock must be held.
    bool held_by_other(SceUID thread_id) const;
    // Takes the flags for the thread if pattern matches them, or returns why it can't. The lock must
    // be held.
    SceInt32 try_take(SceUID thread_id, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits);
    // Completes wait if the flags match its pattern: writes its out bits, clears the flags its mode
    // asks for and releases its hold. Returns false if they don't match. The lock must be held.
    bool take(const WaitRequest &wait);
    // Gives the flags to the waits they match, in queue order.
    void serve() override;
    // Gives a wait that timed out the flag's bits and gives back its hold.
    void wait_end(ThreadState &thread, SceInt32 result) override;
    // Gives back one hold of the holder, the wait that took it ended. The lock must be held.
    void drop_hold();
    // Makes thread the holder and lists the flag in what it holds. The lock must be held.
    void set_holder(ThreadState &thread);
    // Leaves the flag with no holder and unlists it. The lock must be held.
    void clear_holder();

    // Guarded by the lock
    SceUInt32 flags;
    // Without SCE_EVENT_WAITMULTIPLE, the thread whose waits hold the flag and how many waits hold
    // it. Other threads can't wait on or poll a held flag. A wait releases its hold when it gets the
    // bits, when a cancel or delete wakes it, or when it times out in the queue. A wait that ends
    // before sleeping without getting the bits, like one with a timeout of 0, keeps its hold until
    // the thread ends or the flag is deleted.
    ThreadState *holder = nullptr;
    int holds = 0;
};

using EventFlagRef = Ref<EventFlag>;

class Condvar : public Waitable {
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

    const MutexRef associated_mutex;

    SceInt32 wait(MemState &mem, ThreadState &thread, SceUInt32 *timeout, bool callbacks);
    SceInt32 signal(SignalTarget target);
    // Returns how many threads wait on the condition variable, 0 once it was deleted.
    SceUInt32 num_waiters() const;

protected:
    Condvar(SceUInt32 attr, const char *name, MutexRef associated_mutex);

private:
    void end_waits(SceUID deleter) override;
    bool lightweight() const { return get_uid_class() == UidClass::lw_cond; }
};

class LwCond final : public WithUidClass<Condvar, UidClass::lw_cond> {
    friend class Ref<LwCond>;

    LwCond(SceUInt32 attr, const char *name, MutexRef associated_mutex, Ptr<SceKernelLwCondWork> workarea);

public:
    // Fills info with what the lightweight condition variable's info reports, or returns
    // UNKNOWN_LW_COND_ID if it was deleted.
    SceInt32 info(SceKernelLwCondInfo &info) const;

    const Ptr<SceKernelLwCondWork> workarea;
};

class HeavyCond final : public WithUidClass<Condvar, UidClass::cond> {
    friend class Ref<HeavyCond>;

    using WithUidClass::WithUidClass;

public:
    // Fills info with what the condition variable's info reports, or returns UNKNOWN_COND_ID if it was
    // deleted.
    SceInt32 info(SceKernelCondInfo &info) const;
};

using CondvarRef = Ref<Condvar>;

// One piece of a message pipe transfer, a buffer in guest memory
struct MsgPipeVector {
    Ptr<void> base;
    SceSize size;
};

// How far a message pipe send or receive got, kept in the caller's frame so that whoever moves
// bytes carries it on
struct MsgPipeTransfer {
    std::span<const MsgPipeVector> pieces;
    MemState *mem;
    // Where the caller gets the size moved, or null
    SceSize *size;
    SceSize total;
    SceSize left;
    bool full;
    // The piece the next byte goes to or comes from, and how far into it
    std::size_t piece = 0;
    SceSize offset = 0;

    MsgPipeTransfer(MemState &mem, std::span<const MsgPipeVector> pieces, SceSize *size, SceSize total, bool full);
    // Returns whether it moved what its mode asks for
    bool done() const;
    // Returns whether it moved part of its bytes, which keeps it at the head of its queue
    bool keeps_place() const;
    // Returns the guest bytes at its position, at most n and within one piece
    std::span<char> run(SceSize n);
    void advance(SceSize n);
    // Writes the size moved so far for the caller
    void report() const;
};

// A message pipe send in progress
struct MsgPipeSender : MsgPipeTransfer {
    using MsgPipeTransfer::MsgPipeTransfer;
};

// A message pipe receive in progress
struct MsgPipeReceiver : MsgPipeTransfer {
    // A DONT_REMOVE receive, which copies bytes without taking them
    bool peek;

    MsgPipeReceiver(MemState &mem, std::span<const MsgPipeVector> pieces, SceSize *size, SceSize total, bool full, bool peek);
};

class MsgPipe final : public WithUidClass<Waitable, UidClass::msg_pipe> {
    friend class Ref<MsgPipe>;

    MsgPipe(SceUInt32 attr, const char *name, std::size_t buf_size);

public:
    // Returns the error a create with these arguments gets, or SCE_KERNEL_OK
    static SceInt32 check_create(const char *name, SceUInt32 type, SceUInt32 attr, SceSize buf_size);

    // Both move the bytes of the pieces in order, waiting until a FULL call moved all of them or an
    // ASAP call any. The size moved so far is kept in size, which may be null.
    SceInt32 receive(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size, SceUInt32 *timeout, bool callbacks);
    SceInt32 send(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size, SceUInt32 *timeout, bool callbacks);
    // Both move what they can at once, without waiting
    SceInt32 try_receive(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size);
    SceInt32 try_send(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size);
    SceInt32 cancel(SceUInt32 *num_send_wait_threads, SceUInt32 *num_receive_wait_threads);
    // Fills info with what the message pipe's info reports, or returns UNKNOWN_MSG_PIPE_ID if it was
    // deleted.
    SceInt32 info(SceKernelMppInfo &info) const;

private:
    void end_waits(SceUID deleter) override;
    // Carries on the waiters' transfers from the head, up to the first that doesn't end
    void serve() override;
    // Returns the first waiting sender or receiver, or null
    ThreadState *first_sender();
    ThreadState *first_receiver();
    // Moves the sender's bytes to the waiting receivers, then puts the rest into the pipe. Returns
    // whether the sender is done.
    bool give(MsgPipeSender &sender);
    // Moves bytes to the receiver from the pipe, then from the waiting senders, which put their rest
    // into the pipe. Returns whether the receiver is done.
    bool take(MsgPipeReceiver &receiver);
    // Puts as much of the sender's rest into the pipe as its mode allows
    void fill(MsgPipeSender &sender);
    // Moves the pipe's bytes to the receiver, up to what it still wants
    void drain(MsgPipeReceiver &receiver);
    // Returns the bytes a receive can get at once, the pipe's and those the waiting senders still have
    SceSize available() const;
    // Copies n bytes from the sender to the receiver, and takes them from the sender unless the
    // receiver peeks
    static void copy(MsgPipeSender &sender, MsgPipeReceiver &receiver, SceSize n);
    // Returns whether a new call with request goes ahead of the calls already waiting on its side
    bool goes_first(const WaitRequest &request, int priority);

    // Guarded by the lock. Senders and receivers wait in the one queue.
    ByteRingBuffer data_buffer;
};

using MsgPipeRef = Ref<MsgPipe>;

// Only the concrete classes are kernel object classes, not the ones they share
static_assert(KernelObjectClass<HeavyMutex> && KernelObjectClass<LwMutex> && !KernelObjectClass<Mutex>);
static_assert(KernelObjectClass<SimpleEvent> && KernelObjectClass<Timer> && !KernelObjectClass<Event>);
