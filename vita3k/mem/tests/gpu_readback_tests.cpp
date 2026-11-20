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

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <new>
#include <dlfcn.h>
#include <iostream>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mem/functions.h>
#include <mem/state.h>
#include <signal.h>
#include <stdexcept>
#include <sys/mman.h>
#include <thread>

static std::atomic<bool> observe{ false }, alias_entered{ false }, fault_entered{ false };
static mach_vm_address_t observed_source = 0;
static struct sigaction previous_bus{}, previous_segv{};
static std::atomic<bool> fail_next_allocation{ false };
void *operator new(std::size_t size) {
    if (fail_next_allocation.exchange(false))
        throw std::bad_alloc();
    if (auto pointer = std::malloc(size ? size : 1))
        return pointer;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *pointer) noexcept { std::free(pointer); }
void operator delete[](void *pointer) noexcept { std::free(pointer); }
void operator delete(void *pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void *pointer, std::size_t) noexcept { std::free(pointer); }
static std::atomic<bool> fail_next_arm{ false };
extern "C" int mprotect(void *address, size_t size, int protection) {
    using Function = int (*)(void *, size_t, int);
    static const auto original = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "mprotect"));
    if (protection == PROT_NONE && fail_next_arm.exchange(false)) {
        errno = EACCES;
        return -1;
    }
    return original(address, size, protection);
}
static void observer(int sig, siginfo_t *info, void *context) {
    if (observe)
        fault_entered = true;
    (sig == SIGBUS ? previous_bus : previous_segv).sa_sigaction(sig, info, context);
}
extern "C" kern_return_t mach_vm_remap(vm_map_t task, mach_vm_address_t *target, mach_vm_size_t size,
    mach_vm_offset_t mask, int flags, vm_map_t source_task, mach_vm_address_t source,
    boolean_t copy, vm_prot_t *current, vm_prot_t *maximum, vm_inherit_t inherit) {
    using Function = decltype(&mach_vm_remap);
    static auto original = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "mach_vm_remap"));
    const auto result = original(task, target, size, mask, flags, source_task, source, copy, current, maximum, inherit);
    if (result == KERN_SUCCESS && observe && source == observed_source) {
        alias_entered = true;
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!fault_entered && std::chrono::steady_clock::now() < end)
            std::this_thread::yield();
    }
    return result;
}
static void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() try {
    MemState mem;
    require(init(mem, false), "init");
    require(gpu_readback_supported(), "macOS backend absent");
    // New allocation-identity metadata must not leak guest pages or escape
    // alloc/try_alloc_at/alloc_aligned when its map node cannot be allocated.
    const auto available = mem_available(mem);
    const auto initial_generations = mem.allocation_generations.size();
    for (int kind = 0; kind < 3; ++kind) {
        fail_next_allocation = true;
        const auto failed = kind == 0 ? alloc(mem, 16384, "metadata failure")
            : kind == 1 ? try_alloc_at(mem, 0x81000000, 16384, "metadata failure")
                        : alloc_aligned(mem, 16384, "metadata failure", 65536);
        require(!fail_next_allocation.exchange(false), "allocation failure was not injected");
        require(!failed && mem_available(mem) == available && mem.allocation_generations.size() == initial_generations, "allocation metadata rollback");
    }
    const auto retry = alloc_aligned(mem, 16384, "metadata retry", 65536);
    require(retry && retry % 65536 == 0 && bool(capture_mem_allocation(mem, retry, 16384)), "aligned metadata retry");
    free(mem, retry);
    require(mem_available(mem) == available, "aligned retry leaked pages");
    struct sigaction action{};
    action.sa_sigaction = observer;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    sigaction(SIGBUS, &action, &previous_bus);
    sigaction(SIGSEGV, &action, &previous_segv);
    const auto base = alloc(mem, 131072, "readback safety");
    require(base != 0, "allocation");
    const auto token = capture_mem_allocation(mem, base, 131072);
    const uint8_t pixels[16] = { 1, 2, 3, 4 };

    fail_next_arm = true;
    require(!begin_gpu_readback_watch(mem, token, base, 16, 1, 16), "failed arm returned valid watch");
    require(mem.readback_watches.empty(), "failed arm leaked registration");
    mem.memory[base] = 0;

    // A real CPU read before commit conservatively cancels ownership.
    auto watch = begin_gpu_readback_watch(mem, token, base, 16, 1, 16);
    require(bool(watch), "read watch");
    const auto prior = *reinterpret_cast<volatile uint8_t *>(mem.memory.get() + base);
    require(watch->invalidated && prior == 0, "CPU read not observed");
    require(!write_gpu_readback_rows(mem, base, pixels, 16, 1, 16, &token, watch.get()), "read conflict committed");
    finish_gpu_readback_watch(mem, watch);

    // Later protection registrations cannot weaken the readback's read trap.
    watch = begin_gpu_readback_watch(mem, token, base, 16, 1, 16);
    unsigned read_callbacks = 0;
    require(add_protect(mem, base, 4096, MemPerm::ReadOnly,
                [&](Address, bool) { ++read_callbacks; return true; }),
        "later RO protection");
    const auto after_ro = *reinterpret_cast<volatile uint8_t *>(mem.memory.get() + base);
    require(watch->invalidated && read_callbacks == 0 && after_ro == 0, "later RO weakened read watch");
    // The existing RO callback is retained until a genuine write.
    mem.memory[base] = 0;
    require(read_callbacks == 1, "existing RO callback changed");
    finish_gpu_readback_watch(mem, watch);

    // Same-value stores must also invalidate; comparing snapshots is insufficient.
    watch = begin_gpu_readback_watch(mem, token, base, 16, 1, 16);
    *reinterpret_cast<volatile uint8_t *>(mem.memory.get() + base) = 0;
    require(watch->invalidated, "same-value write not observed");
    require(!write_gpu_readback_rows(mem, base, pixels, 16, 1, 16, &token, watch.get()), "write conflict committed");
    require(mem.memory[base] == 0, "CPU byte overwritten");
    finish_gpu_readback_watch(mem, watch);

    // Shared-page unrelated access is a conservative cancellation, never a lost store.
    watch = begin_gpu_readback_watch(mem, token, base, 16, 1, 16);
    mem.memory[base + mem.host_page_size / 2] = 0x69;
    require(watch->invalidated, "shared-page write missed");
    require(!write_gpu_readback_rows(mem, base, pixels, 16, 1, 16, &token, watch.get()), "shared-page conflict committed");
    finish_gpu_readback_watch(mem, watch);

    // During the commit a real fault waits at the existing protection mutex.
    // We observe fault entry without waiting for the writer to complete.
    for (const auto permission : { MemPerm::ReadOnly, MemPerm::None }) {
        std::atomic<unsigned> callbacks{ 0 };
        require(add_protect(mem, base, mem.host_page_size, permission,
                    [&](Address, bool) { ++callbacks; return true; }),
            "protection");
        watch = begin_gpu_readback_watch(mem, token, base, 16, 1, 16);
        alias_entered = false;
        fault_entered = false;
        observed_source = reinterpret_cast<mach_vm_address_t>(mem.memory.get() + base);
        observe = true;
        std::thread guest([&] {
            const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (!alias_entered && std::chrono::steady_clock::now() < end)
                std::this_thread::yield();
            if (alias_entered)
                *reinterpret_cast<volatile uint8_t *>(mem.memory.get() + base) = 0x5a;
        });
        const bool committed = write_gpu_readback_rows(mem, base, pixels, 16, 1, 16, &token, watch.get());
        guest.join();
        observe = false;
        require(committed && fault_entered, "during-commit guest write did not trap");
        require(callbacks == 1 && mem.memory[base] == 0x5a, "callback or latest post-commit write lost");
        finish_gpu_readback_watch(mem, watch);
    }

    // Completed and abandoned watches neither leak nor weaken existing callbacks.
    for (unsigned i = 0; i < 128; ++i) {
        watch = begin_gpu_readback_watch(mem, token, base, 16, 1, 16);
        require(write_gpu_readback_rows(mem, base, pixels, 16, 1, 16, &token, watch.get()), "repeat commit");
        finish_gpu_readback_watch(mem, watch);
        require(mem.readback_watches.empty(), "registration leak");
    }
    watch = begin_gpu_readback_watch(mem, token, base, 16, 1, 16);
    watch.reset();
    const auto abandoned = *reinterpret_cast<volatile uint8_t *>(mem.memory.get() + base);
    require(abandoned == 1 && mem.readback_watches.empty(), "abandoned watch cleanup");

    // Cache identity is captured before freeing A, not freshly captured from B.
    watch = begin_gpu_readback_watch(mem, token, base, 16, 1, 16);
    free(mem, base);
    require(watch->invalidated, "free did not cancel watch");
    require(try_alloc_at(mem, base, 131072, "replacement") == base, "replacement");
    require(!matches_mem_allocation(mem, token, base, 16), "stale cache identity matched B");
    require(!write_gpu_readback_rows(mem, base, pixels, 16, 1, 16, &token, watch.get()), "A wrote into B");
    require(mem.memory[base] == 0, "replacement changed");
    finish_gpu_readback_watch(mem, watch);
    free(mem, base);
    const auto unaligned = try_alloc_at(mem, 0x9000107b, 8192, "unaligned identity");
    require(unaligned == 0x9000107b, "unaligned alloc_at");
    const auto unaligned_token = capture_mem_allocation(mem, unaligned, 8192);
    require(bool(unaligned_token), "unaligned token");
    free(mem, unaligned);
    require(!matches_mem_allocation(mem, unaligned_token, unaligned, 16), "unaligned free retained token");
    deinit_mem(mem);
    require(init(mem, false), "reinit");
    require(try_alloc_at(mem, base, 131072, "new lifetime") == base, "reinit allocation");
    require(!matches_mem_allocation(mem, token, base, 16), "reinit reused identity");
    free(mem, base);
    std::cout << "{\"passed\":true,\"real_faults_during_alias_commit\":2,\"read_and_same_value_write_conflicts\":true,\"shared_page_conflict_cancel\":true,\"permission_failure_fail_closed\":true,\"later_protect_preserves_watch\":true,\"repeat_watch_cycles\":128,\"cache_allocation_reuse_and_reinit\":true}" << std::endl;
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << std::endl;
    return 1;
}
