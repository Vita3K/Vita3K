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

#include <mem/functions.h>
#include <mem/state.h>

#include <util/align.h>
#include <util/log.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iterator>
#include <mutex>
#include <new>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <csignal>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

constexpr uint32_t STANDARD_PAGE_SIZE = KiB(4);
constexpr size_t TOTAL_MEM_SIZE = GiB(4);
constexpr bool LOG_PROTECT = false;
#ifdef NDEBUG
constexpr bool PAGE_NAME_TRACKING = false;
#else
constexpr bool PAGE_NAME_TRACKING = true;
#endif

// TODO: support multiple handlers
static AccessViolationHandler access_violation_handler;
static void register_access_violation_handler(const AccessViolationHandler &handler);

static Address alloc_inner(MemState &state, uint32_t start_page, uint32_t page_count, const char *name, const bool force);
static void delete_memory(uint8_t *memory);
static void cancel_readbacks_locked(MemState &state, uint64_t begin, uint64_t end);
static void prune_readbacks_locked(MemState &state);
static MemPerm readback_page_permission(const MemState &state, uint64_t page);
static bool protect_checked(MemState &state, Address addr, uint32_t size, MemPerm perm);

#ifdef _WIN32
static std::string get_error_msg() {
    return std::system_category().message(GetLastError());
}
#else
static std::string get_error_msg() {
    return strerror(errno);
}
#endif

bool init(MemState &state, const bool use_page_table) {
#ifdef _WIN32
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    state.host_page_size = system_info.dwPageSize;
#else
    state.host_page_size = static_cast<int>(sysconf(_SC_PAGESIZE));
#endif

    assert(state.host_page_size >= 4096); // Limit imposed by Unicorn.

    state.invalid_access_logged.clear();

    void *preferred_address = reinterpret_cast<void *>(1ULL << 34);

#ifdef _WIN32
    state.memory = Memory(static_cast<uint8_t *>(VirtualAlloc(preferred_address, TOTAL_MEM_SIZE, MEM_RESERVE, PAGE_NOACCESS)), delete_memory);
    if (!state.memory) {
        // fallback
        state.memory = Memory(static_cast<uint8_t *>(VirtualAlloc(nullptr, TOTAL_MEM_SIZE, MEM_RESERVE, PAGE_NOACCESS)), delete_memory);

        if (!state.memory) {
            LOG_CRITICAL("VirtualAlloc failed: {}", get_error_msg());
            return false;
        }
    }
#else
    // http://man7.org/linux/man-pages/man2/mmap.2.html
    const int prot = PROT_NONE;
    const int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    const int fd = 0;
    const off_t offset = 0;
    // preferred_address is only a hint for mmap, if it can't use it, the kernel will choose itself the address
    state.memory = Memory(static_cast<uint8_t *>(mmap(preferred_address, TOTAL_MEM_SIZE, prot, flags, fd, offset)), delete_memory);
    if (state.memory.get() == MAP_FAILED) {
        LOG_CRITICAL("mmap failed {}", get_error_msg());
        return false;
    }
#endif

    const size_t table_length = TOTAL_MEM_SIZE / STANDARD_PAGE_SIZE;
    state.alloc_table = AllocPageTable(new AllocMemPage[table_length]);
    memset(state.alloc_table.get(), 0, sizeof(AllocMemPage) * table_length);

    state.allocator.set_maximum(table_length);

    const auto handler = [&state](uint8_t *addr, bool write) noexcept {
        return handle_access_violation(state, addr, write);
    };
    register_access_violation_handler(handler);

    const Address null_address = alloc_inner(state, 0, state.host_page_size / STANDARD_PAGE_SIZE, "null", true);
    assert(null_address == 0);
#ifdef _WIN32
    DWORD old_protect = 0;
    const BOOL ret = VirtualProtect(state.memory.get(), state.host_page_size, PAGE_NOACCESS, &old_protect);
    LOG_CRITICAL_IF(!ret, "VirtualAlloc failed: {}", get_error_msg());
#else
    const int ret = mprotect(state.memory.get(), state.host_page_size, PROT_NONE);
    LOG_CRITICAL_IF(ret == -1, "mprotect failed: {}", get_error_msg());
#endif

    state.use_page_table = use_page_table;
    if (use_page_table) {
        state.page_table = PageTable(new PagePtr[TOTAL_MEM_SIZE / KiB(4)]);
        // we use an absolute offset (it is faster), so each entry is the same
        std::fill_n(state.page_table.get(), TOTAL_MEM_SIZE / KiB(4), state.memory.get());
    }

    return true;
}

static void delete_memory(uint8_t *memory) {
    if (memory != nullptr) {
#ifdef _WIN32
        const BOOL ret = VirtualFree(memory, 0, MEM_RELEASE);
        assert(ret);
#else
        munmap(memory, TOTAL_MEM_SIZE);
#endif
    }
}

bool is_valid_addr(const MemState &state, Address addr) {
    const uint32_t page_num = addr / STANDARD_PAGE_SIZE;
    return addr && state.allocator.free_slot_count(page_num, page_num + 1) == 0;
}

bool is_valid_addr_range(const MemState &state, Address start, Address end) {
    const uint32_t start_page = start / STANDARD_PAGE_SIZE;
    const uint32_t end_page = (end + STANDARD_PAGE_SIZE - 1) / STANDARD_PAGE_SIZE;
    return state.allocator.free_slot_count(start_page, end_page) == 0;
}

static Address alloc_inner(MemState &state, uint32_t start_page, uint32_t page_count, const char *name, const bool force) {
    int page_num;
    if (force) {
        if (state.allocator.allocate_at(start_page, page_count) < 0) {
            return 0;
        }
        page_num = start_page;
    } else {
        page_num = state.allocator.allocate_from(start_page, page_count, false);
        if (page_num < 0)
            return 0;
    }

    const uint32_t size = page_count * STANDARD_PAGE_SIZE;
    const Address addr = page_num * STANDARD_PAGE_SIZE;

    // Publish allocation identity before changing permissions or page metadata.
    // A host allocation failure must return the bitmap reservation to the pool.
    try {
        state.allocation_generations.emplace(addr, state.next_allocation_generation + 1);
    } catch (const std::bad_alloc &) {
        state.allocator.free(page_num, page_count);
        return 0;
    }
    ++state.next_allocation_generation;

    const Address commit_start = align_down(addr, state.host_page_size);
    const Address commit_end = align(addr + size, state.host_page_size);
    const uint32_t commit_size = commit_end - commit_start;
    uint8_t *const commit_ptr = &state.memory[commit_start];
    // An allocator permission change must not leave an older readback armed.
    {
        const std::lock_guard<std::mutex> lock(state.protect_mutex);
        cancel_readbacks_locked(state, commit_start, uint64_t(commit_start) + commit_size);
    }

    // Make memory chunk available to access
#ifdef _WIN32
    const void *const ret = VirtualAlloc(commit_ptr, commit_size, MEM_COMMIT, PAGE_READWRITE);
    LOG_CRITICAL_IF(!ret, "VirtualAlloc failed: {}", get_error_msg());
#else
    const int ret = mprotect(commit_ptr, commit_size, PROT_READ | PROT_WRITE);
    LOG_CRITICAL_IF(ret == -1, "mprotect failed: {}", get_error_msg());
#endif
    std::memset(&state.memory[addr], 0, size);

    AllocMemPage &page = state.alloc_table[page_num];
    assert(!page.allocated);
    page.allocated = 1;
    page.size = page_count;

    if (PAGE_NAME_TRACKING) {
        state.page_name_map.emplace(page_num, name);
    }

    return addr;
}

Address alloc_aligned(MemState &state, uint32_t size, const char *name, unsigned int alignment, Address start_addr) {
    if (alignment == 0)
        return alloc(state, size, name, start_addr);
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    size += alignment;
    const uint32_t page_count = align(size, STANDARD_PAGE_SIZE) / STANDARD_PAGE_SIZE;
    const Address addr = alloc_inner(state, start_addr / STANDARD_PAGE_SIZE, page_count, name, false);
    if (!addr)
        return 0;
    const Address align_addr = align(addr, alignment);
    const uint32_t page_num = addr / STANDARD_PAGE_SIZE;
    const uint32_t align_page_num = align_addr / STANDARD_PAGE_SIZE;

    if (page_num != align_page_num) {
        AllocMemPage &page = state.alloc_table[page_num];
        AllocMemPage &align_page = state.alloc_table[align_page_num];
        const uint32_t remnant_front = align_page_num - page_num;
        state.allocator.free(page_num, remnant_front);
        page.allocated = 0;
        align_page.allocated = 1;
        align_page.size = page.size - remnant_front;
        // Re-key the existing node: alignment must not introduce another
        // potentially throwing host allocation after the guest pages commit.
        auto generation = state.allocation_generations.extract(addr);
        generation.key() = align_page_num * STANDARD_PAGE_SIZE;
        state.allocation_generations.insert(std::move(generation));
    }

    return align_addr;
}

static void align_to_page(MemState &state, Address &addr, Address &size) {
    const Address end = align(addr + size, STANDARD_PAGE_SIZE);
    addr = align_down(addr, STANDARD_PAGE_SIZE);
    size = end - addr;
}

static void unprotect_unchecked(MemState &state, Address addr, uint32_t size) {
    if (LOG_PROTECT) {
        fmt::print("Unprotect: {} {}\n", log_hex(addr), size);
    }
    uint8_t *addr_ptr = state.use_page_table ? state.page_table[addr / KiB(4)] : state.memory.get();

    uint8_t *target = &addr_ptr[addr];
    uint8_t *aligned_start = reinterpret_cast<uint8_t *>(
        align_down(reinterpret_cast<uintptr_t>(target), state.host_page_size));
    uint8_t *aligned_end = reinterpret_cast<uint8_t *>(align(reinterpret_cast<uintptr_t>(target + size), state.host_page_size));
    size_t aligned_size = aligned_end - aligned_start;

#ifdef _WIN32
    DWORD old_protect = 0;
    const BOOL ret = VirtualProtect(aligned_start, aligned_size, PAGE_READWRITE, &old_protect);
    LOG_CRITICAL_IF(!ret, "VirtualAlloc failed: {}", get_error_msg());
#else
    const int ret = mprotect(aligned_start, aligned_size, PROT_READ | PROT_WRITE);
    LOG_CRITICAL_IF(ret == -1, "mprotect failed: {}", get_error_msg());
#endif
}

// Retire software-cache registrations before opening their shared host pages.
// Access/abort registrations are deliberately not invoked for collateral access.
// Callbacks have the same mutex/no-renderer-wait contract as ordinary faults.
// Erase and re-key existing nodes only: no temporary container or allocation in
// the fault handler. Affected pages outside the requested opening are restored
// from their remaining registrations rather than opened unconditionally.
static unsigned retire_cache_protections_locked(MemState &state, uint64_t begin, uint64_t end, Address fault,
    uint64_t &affected_begin, uint64_t &affected_end) {
    // External page-table mappings may have noncontiguous backing. Preserve
    // their existing path; this fix is for the original unmapped guest view.
    if (state.use_page_table || begin >= end)
        return 0;
    unsigned retired = 0;
    auto it = state.protect_tree.lower_bound(Address(end - 1));
    while (it != state.protect_tree.end()) {
        auto &segment = it->second;
        if (uint64_t(it->first) + segment.size <= begin)
            break;
        bool changed = false;
        for (auto block = segment.blocks.begin(); block != segment.blocks.end();) {
            const auto block_begin = align_down(uint64_t(block->first), uint64_t(state.host_page_size));
            const auto block_end = align(uint64_t(block->first) + block->second.size, uint64_t(state.host_page_size));
            if (!block->second.cache_invalidation || block_begin >= end || block_end <= begin) {
                ++block;
                continue;
            }
            affected_begin = std::min(affected_begin, block_begin);
            affected_end = std::max(affected_end, block_end);
            // Opening the block permits future writes even for a read fault.
            // Notify caches conservatively before discarding their registration.
            block->second.callback(fault, true);
            block = segment.blocks.erase(block);
            ++retired;
            changed = true;
        }
        auto next = std::next(it);
        if (changed) {
            if (segment.blocks.empty()) {
                state.protect_tree.erase(it);
            } else {
                const auto first = segment.blocks.begin()->first;
                uint64_t last = first;
                auto perm = MemPerm::ReadWrite;
                for (const auto &[address, block] : segment.blocks) {
                    last = std::max(last, uint64_t(address) + block.size);
                    perm = most_restrictive_perm(perm, block.perm);
                }
                segment.size = uint32_t(last - first);
                segment.perm = perm;
                auto node = state.protect_tree.extract(it);
                node.key() = first;
                state.protect_tree.insert(std::move(node));
            }
        }
        it = next;
    }
    return retired;
}

static void restore_registered_permissions_locked(MemState &state, uint64_t begin, uint64_t end) {
    if (begin >= end)
        return;
    uint64_t run_begin = begin;
    auto permission = readback_page_permission(state, begin);
    const uint64_t max_batch = align_down(uint64_t(UINT32_MAX), uint64_t(state.host_page_size));
    for (uint64_t page = begin + state.host_page_size; page <= end; page += state.host_page_size) {
        const auto next = page < end ? readback_page_permission(state, page) : permission;
        if (page < end && next == permission && page - run_begin < max_batch)
            continue;
        if (!protect_checked(state, Address(run_begin), uint32_t(page - run_begin), permission)) {
            LOG_CRITICAL("Could not restore collateral cache page permissions");
            std::terminate();
        }
        run_begin = page;
        permission = next;
    }
}

static unsigned unprotect_with_cache_invalidation_locked(MemState &state, uint64_t begin, uint64_t end, Address fault) {
    if (begin >= end)
        return 0;
    uint64_t affected_begin = begin, affected_end = end;
    const unsigned retired = retire_cache_protections_locked(state, begin, end, fault, affected_begin, affected_end);
    cancel_readbacks_locked(state, begin, end);
    restore_registered_permissions_locked(state, affected_begin, begin);
    restore_registered_permissions_locked(state, end, affected_end);
    // Keep the uint32_t lower-level API valid even if registrations cover the
    // entire 4GiB address space through overlapping host pages.
    const uint64_t max_batch = align_down(uint64_t(UINT32_MAX), uint64_t(state.host_page_size));
    while (begin < end) {
        const auto size = std::min(end - begin, max_batch);
        unprotect_unchecked(state, Address(begin), uint32_t(size));
        begin += size;
    }
    return retired;
}

void unprotect_inner(MemState &state, Address addr, uint32_t size) {
    const std::lock_guard<std::mutex> lock(state.protect_mutex);
    if (state.use_page_table) {
        // Select the backing with the original guest address. Rounding the
        // address first can select a different external page-table mapping.
        unprotect_unchecked(state, addr, size);
        return;
    }
    unprotect_with_cache_invalidation_locked(state,
        align_down(uint64_t(addr), uint64_t(state.host_page_size)),
        align(uint64_t(addr) + size, uint64_t(state.host_page_size)), addr);
}

static bool protect_checked(MemState &state, Address addr, uint32_t size, const MemPerm perm) {
    uint8_t *addr_ptr = state.use_page_table ? state.page_table[addr / KiB(4)] : state.memory.get();

    uint8_t *target = &addr_ptr[addr];
    uint8_t *aligned_start = reinterpret_cast<uint8_t *>(
        align_down(reinterpret_cast<uintptr_t>(target), state.host_page_size));
    uint8_t *aligned_end = reinterpret_cast<uint8_t *>(align(reinterpret_cast<uintptr_t>(target + size), state.host_page_size));
    size_t aligned_size = aligned_end - aligned_start;

#ifdef _WIN32
    DWORD old_protect = 0;
    const BOOL ret = VirtualProtect(aligned_start, aligned_size, (perm == MemPerm::None) ? PAGE_NOACCESS : ((perm == MemPerm::ReadOnly) ? PAGE_READONLY : PAGE_READWRITE), &old_protect);
    LOG_CRITICAL_IF(!ret, "VirtualAlloc failed: {}", get_error_msg());
    return ret != 0;
#else
    const int ret = mprotect(aligned_start, aligned_size, (perm == MemPerm::None) ? PROT_NONE : ((perm == MemPerm::ReadOnly) ? PROT_READ : (PROT_READ | PROT_WRITE)));
    LOG_CRITICAL_IF(ret == -1, "mprotect failed: {}", get_error_msg());
    return ret == 0;
#endif
}

void protect_inner(MemState &state, Address addr, uint32_t size, const MemPerm perm) {
    protect_checked(state, addr, size, perm);
}

bool handle_access_violation(MemState &state, uint8_t *addr, bool write) noexcept {
    const uintptr_t memory_addr = reinterpret_cast<uintptr_t>(state.memory.get());
    const uintptr_t fault_addr = reinterpret_cast<uintptr_t>(addr);

    Address vaddr = 0;
    const std::unique_lock<std::mutex> lock(state.protect_mutex);
    if (fault_addr < memory_addr || fault_addr >= memory_addr + TOTAL_MEM_SIZE) {
        if (state.use_page_table) {
            // this may come from an external mapping
            uint64_t addr_val = std::bit_cast<uint64_t>(addr);
            auto it = state.external_mapping.lower_bound(addr_val);
            if (it != state.external_mapping.end() && addr_val < it->first + it->second.size) {
                vaddr = static_cast<Address>(addr_val - it->first + it->second.address);
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else {
        vaddr = static_cast<Address>(fault_addr - memory_addr);
    }

    if (!is_valid_addr(state, vaddr)) {
        return false;
    }
    if (LOG_PROTECT) {
        fmt::print("Access: {}\n", log_hex(vaddr));
    }

    const uint64_t fault_page = align_down(uint64_t(vaddr), uint64_t(state.host_page_size));
    bool watched_page = false;
    for (const auto &registration : state.readback_watches) {
        if (fault_page < registration.end && fault_page + state.host_page_size > registration.begin)
            watched_page = true;
    }
    if (watched_page) {
        cancel_readbacks_locked(state, fault_page, fault_page + state.host_page_size);
        // A read trap added solely for readback needs no existing CPU callback.
        // Restore the original strongest page permission, not unconditional RW.
        MemPerm original = MemPerm::ReadWrite;
        for (const auto &[start, segment] : state.protect_tree) {
            if (uint64_t(start) < fault_page + state.host_page_size && uint64_t(start) + segment.size > fault_page)
                original = most_restrictive_perm(original, segment.perm);
        }
        if (original == MemPerm::ReadWrite || (!write && original == MemPerm::ReadOnly))
            return true;
    }

    auto it = state.protect_tree.lower_bound(vaddr);
    if (it == state.protect_tree.end()) {
        // HACK: keep going
        const auto begin = align_down(uint64_t(vaddr), uint64_t(state.host_page_size));
        if (!unprotect_with_cache_invalidation_locked(state, begin, begin + state.host_page_size, vaddr))
            LOG_CRITICAL("Unhandled write protected region was valid. Address=0x{:X}", vaddr);
        return true;
    }

    ProtectSegmentInfo &info = it->second;
    if (vaddr < it->first || vaddr >= it->first + info.size) {
        // HACK: keep going
        const auto begin = align_down(uint64_t(vaddr), uint64_t(state.host_page_size));
        if (!unprotect_with_cache_invalidation_locked(state, begin, begin + state.host_page_size, vaddr))
            LOG_CRITICAL("Unhandled write protected region was valid. Address=0x{:X}", vaddr);
        return true;
    }

    Address previous_beg = it->first;
    for (auto &[block_addr, block] : info.blocks) {
        block.callback(vaddr, write);
    }

    const auto original_address = it->first;
    const auto original_size = info.size;
    const auto begin = align_down(uint64_t(original_address), uint64_t(state.host_page_size));
    const auto end = align(uint64_t(original_address) + original_size, uint64_t(state.host_page_size));
    state.protect_tree.erase(it);
    if (state.use_page_table) {
        unprotect_unchecked(state, original_address, original_size);
        return true;
    }
    // Opening a logical segment can also release neighboring cache blocks and
    // watches on the same host pages; retire those before exposing writable memory.
    unprotect_with_cache_invalidation_locked(state, begin, end, vaddr);

    return true;
}

static bool add_protect_impl(MemState &state, Address addr, const uint32_t size, const MemPerm perm, const ProtectCallback &callback, bool cache_invalidation) {
    const std::lock_guard<std::mutex> lock(state.protect_mutex);
    ProtectSegmentInfo protect(size, perm);
    align_to_page(state, addr, protect.size);

    ProtectBlockInfo block;
    block.size = protect.size;
    block.perm = perm;
    block.cache_invalidation = cache_invalidation;
    block.callback = callback;

    protect.blocks.emplace(addr, std::move(block));

    auto it = state.protect_tree.lower_bound(addr);
    if (it == state.protect_tree.end() || it->first + it->second.size <= addr) {
        if (it == state.protect_tree.begin())
            it = state.protect_tree.end();
        else
            --it;
    }

    while (it != state.protect_tree.end() && it->first < addr + size) {
        const Address start = std::min(it->first, addr);
        protect.size = std::max(it->first + it->second.size, addr + protect.size) - start;
        addr = start;
        protect.blocks.merge(it->second.blocks); // transfer blocks to the new protect
        protect.perm = most_restrictive_perm(protect.perm, it->second.perm);

        if (it == state.protect_tree.begin()) {
            state.protect_tree.erase(it);
            break;
        }

        // protect tree is in reverse order, so decrease it
        state.protect_tree.erase(it--);
    }

    const auto protected_size = protect.size;
    const auto requested_permission = protect.perm;
    state.protect_tree.emplace(addr, std::move(protect));
    if (state.readback_watches.empty()) {
        protect_inner(state, addr, protected_size, requested_permission);
    } else {
        // A new texture/other registration must not weaken an active readback
        // watch, including another logical segment on the same host page.
        const uint64_t end = align(uint64_t(addr) + protected_size, uint64_t(state.host_page_size));
        for (uint64_t page = align_down(uint64_t(addr), uint64_t(state.host_page_size)); page < end; page += state.host_page_size)
            protect_inner(state, Address(page), state.host_page_size, readback_page_permission(state, page));
    }
    return true;
}

bool add_protect(MemState &state, Address addr, uint32_t size, MemPerm perm, const ProtectCallback &callback) {
    return add_protect_impl(state, addr, size, perm, callback, false);
}

bool add_cache_protect(MemState &state, Address addr, uint32_t size, MemPerm perm, const ProtectCallback &callback) {
    return add_protect_impl(state, addr, size, perm, callback, true);
}

bool is_protecting(MemState &state, Address addr, MemPerm *perm) {
    const std::lock_guard<std::mutex> lock(state.protect_mutex);
    auto ite = state.protect_tree.lower_bound(addr);

    if (ite != state.protect_tree.end() && addr < ite->first + ite->second.size) {
        if (perm)
            *perm = ite->second.perm;

        return true;
    }

    return false;
}

void add_external_mapping(MemState &mem, Address addr, uint32_t size, uint8_t *addr_ptr) {
    assert((size & 4095) == 0);
    if (!mem.use_page_table)
        return;

    uint64_t addr_value = std::bit_cast<uint64_t>(addr_ptr);
    uint8_t *page_table_entry = addr_ptr - addr;
    uint8_t *original_address = &mem.memory[addr];
    for (int block = 0; block < size / KiB(4); block++) {
        // this is not thread write safe, but hopefully not other thread is busy copying while this happens
        memcpy(addr_ptr + block * KiB(4), original_address + block * KiB(4), KiB(4));
        mem.page_table[addr / KiB(4) + block] = page_table_entry;
    }

    // set the first page table entry to the original value to be able to call protect_inner
    mem.page_table[addr / KiB(4)] = mem.memory.get();
    protect_inner(mem, addr, size, MemPerm::None);
    mem.page_table[addr / KiB(4)] = page_table_entry;

    const std::unique_lock<std::mutex> lock(mem.protect_mutex);
    mem.external_mapping[addr_value] = { addr, size };
}

void remove_external_mapping(MemState &mem, uint8_t *addr_ptr, uint32_t size) {
    uint64_t addr_value = std::bit_cast<uint64_t>(addr_ptr);
    MemExternalMapping mapping;
    if (mem.use_page_table) {
        const std::unique_lock<std::mutex> lock(mem.protect_mutex);
        auto it = mem.external_mapping.find(addr_value);
        assert(it != mem.external_mapping.end());

        mapping = it->second;
        mem.external_mapping.erase(it);
    } else {
        mapping.address = static_cast<Address>(addr_ptr - mem.memory.get());
        mapping.size = size;
    }

    // remove all protections on this range
    unprotect_inner(mem, mapping.address, mapping.size);
    {
        const std::unique_lock<std::mutex> lock(mem.protect_mutex);
        auto prot_it = mem.protect_tree.lower_bound(mapping.address);
        if (prot_it == mem.protect_tree.end() || prot_it->first + prot_it->second.size <= mapping.address) {
            if (prot_it == mem.protect_tree.begin())
                prot_it = mem.protect_tree.end();
            else
                --prot_it;
        }

        while (prot_it != mem.protect_tree.end() && prot_it->first < mapping.address + mapping.size) {
            if (prot_it == mem.protect_tree.begin()) {
                mem.protect_tree.erase(prot_it);
                break;
            }

            mem.protect_tree.erase(prot_it--);
        }
    }

    if (mem.use_page_table) {
        // unprotect the original memory range
        mem.page_table[mapping.address / KiB(4)] = mem.memory.get();
        unprotect_inner(mem, mapping.address, mapping.size);
        // copy back and reset the page table
        for (int block = 0; block < mapping.size / KiB(4); block++) {
            // this is not thread write safe, but hopefully not other thread is busy copying while this happens
            memcpy(&mem.memory[mapping.address] + block * KiB(4), addr_ptr + block * KiB(4), KiB(4));
            mem.page_table[mapping.address / KiB(4) + block] = mem.memory.get();
        }
    }
}

Address alloc(MemState &state, uint32_t size, const char *name, Address start_addr) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    const uint32_t page_count = align(size, STANDARD_PAGE_SIZE) / STANDARD_PAGE_SIZE;
    const Address addr = alloc_inner(state, start_addr / STANDARD_PAGE_SIZE, page_count, name, false);
    return addr;
}

Address alloc_at(MemState &state, Address address, uint32_t size, const char *name) {
    auto addr = try_alloc_at(state, address, size, name);
    LOG_CRITICAL_IF(addr == 0, "Failed to allocate at specific page. Memory address:{}, size:{}, name:{}", log_hex(address), log_hex(size), name);
    return addr;
}

Address try_alloc_at(MemState &state, Address address, uint32_t size, const char *name) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    const uint32_t wanted_page = address / STANDARD_PAGE_SIZE;
    size += address % STANDARD_PAGE_SIZE;
    const uint32_t page_count = align(size, STANDARD_PAGE_SIZE) / STANDARD_PAGE_SIZE;
    const Address addr = alloc_inner(state, wanted_page, page_count, name, true);
    return addr ? address : 0;
}

Block alloc_block(MemState &mem, uint32_t size, const char *name, Address start_addr) {
    const Address address = alloc(mem, size, name, start_addr);
    return Block(address, [&mem](Address stack) {
        free(mem, stack);
    });
}

void free(MemState &state, Address address) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    const uint32_t page_num = address / STANDARD_PAGE_SIZE;
    assert(page_num >= 0);

    AllocMemPage &page = state.alloc_table[page_num];
    if (!page.allocated) {
        LOG_CRITICAL("Freeing unallocated page");
    }
    {
        const std::lock_guard<std::mutex> lock(state.protect_mutex);
        cancel_readbacks_locked(state, uint64_t(page_num) * STANDARD_PAGE_SIZE,
            uint64_t(page_num + page.size) * STANDARD_PAGE_SIZE);
    }
    page.allocated = 0;
    state.allocation_generations.erase(page_num * STANDARD_PAGE_SIZE);

    state.allocator.free(page_num, page.size);
    if (PAGE_NAME_TRACKING) {
        state.page_name_map.erase(page_num);
    }

    assert(!state.use_page_table || state.page_table[address / KiB(4)] == state.memory.get());
    const Address region_start = page_num * STANDARD_PAGE_SIZE;
    const Address region_end = region_start + page.size * STANDARD_PAGE_SIZE;

    Address host_page = align_down(region_start, state.host_page_size);
    Address batch_start = 0;
    uint32_t batch_size = 0;

    while (host_page < region_end) {
        Address host_page_end = host_page + state.host_page_size;
        uint32_t first_guest = host_page / STANDARD_PAGE_SIZE;
        uint32_t last_guest = host_page_end / STANDARD_PAGE_SIZE;

        if (state.allocator.free_slot_count(first_guest, last_guest) == (last_guest - first_guest)) {
            if (batch_size == 0)
                batch_start = host_page;
            batch_size += state.host_page_size;
        } else if (batch_size > 0) {
            uint8_t *memory = &state.memory[batch_start];
#ifdef _WIN32
            const BOOL ret = VirtualFree(memory, batch_size, MEM_DECOMMIT);
            LOG_CRITICAL_IF(!ret, "VirtualFree failed: {}", get_error_msg());
#else
            int ret = mprotect(memory, batch_size, PROT_NONE);
            LOG_CRITICAL_IF(ret == -1, "mprotect failed: {}", get_error_msg());
            ret = madvise(memory, batch_size, MADV_DONTNEED);
            LOG_CRITICAL_IF(ret == -1, "madvise failed: {}", get_error_msg());
#endif
            batch_size = 0;
        }
        host_page = host_page_end;
    }

    if (batch_size > 0) {
        uint8_t *memory = &state.memory[batch_start];
#ifdef _WIN32
        const BOOL ret = VirtualFree(memory, batch_size, MEM_DECOMMIT);
        LOG_CRITICAL_IF(!ret, "VirtualFree failed: {}", get_error_msg());
#else
        int ret = mprotect(memory, batch_size, PROT_NONE);
        LOG_CRITICAL_IF(ret == -1, "mprotect failed: {}", get_error_msg());
        ret = madvise(memory, batch_size, MADV_DONTNEED);
        LOG_CRITICAL_IF(ret == -1, "madvise failed: {}", get_error_msg());
#endif
    }
}

uint32_t mem_available(MemState &state) {
    return state.allocator.free_slot_count(0, state.allocator.max_offset) * STANDARD_PAGE_SIZE;
}

const char *mem_name(Address address, MemState &state) {
    if (PAGE_NAME_TRACKING) {
        return state.page_name_map.find(address / STANDARD_PAGE_SIZE)->second.c_str();
    }
    return "";
}

void deinit_mem(MemState &state) {
    const std::lock_guard<std::mutex> gen_lock(state.generation_mutex);

    {
        const std::lock_guard<std::mutex> prot_lock(state.protect_mutex);
        state.protect_tree.clear();
        for (const auto &registration : state.readback_watches)
            if (auto watch = registration.watch.lock())
                watch->invalidated = true;
        state.readback_watches.clear();
    }

    state.memory.reset();
    state.alloc_table.reset();
    state.allocation_generations.clear();
    state.allocator.reset();
    state.page_name_map.clear();
    state.page_table.reset();
    state.external_mapping.clear();
    state.use_page_table = false;
    state.host_page_size = 0;
}

#ifdef _WIN32

static LONG WINAPI exception_handler(PEXCEPTION_POINTERS pExp) noexcept {
    if (pExp->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT && IsDebuggerPresent()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto ptr = reinterpret_cast<uint8_t *>(pExp->ExceptionRecord->ExceptionInformation[1]);
    const bool is_writing = pExp->ExceptionRecord->ExceptionInformation[0] == 1;
    const bool is_executing = pExp->ExceptionRecord->ExceptionInformation[0] == 8;

    if (pExp->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && !is_executing) {
        if (access_violation_handler(ptr, is_writing)) {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

static void register_access_violation_handler(const AccessViolationHandler &handler) {
    access_violation_handler = handler;
    if (!AddVectoredExceptionHandler(1, exception_handler)) {
        LOG_CRITICAL("Failed to register an exception handler");
    }
}

#else

static void signal_handler(int sig, siginfo_t *info, void *uct) noexcept {
    auto context = static_cast<ucontext_t *>(uct);

#ifdef __aarch64__
#ifdef __APPLE__
    const uint32_t esr = context->uc_mcontext->__es.__esr;
#else
    _aarch64_ctx *ctx = reinterpret_cast<_aarch64_ctx *>(context->uc_mcontext.__reserved);
    // get the ESR register
    while (ctx->magic != ESR_MAGIC) {
        if (ctx->magic == 0)
            [[unlikely]]
            raise(SIGTRAP);
        else
            [[likely]]
            ctx = reinterpret_cast<_aarch64_ctx *>(reinterpret_cast<uint8_t *>(ctx) + ctx->size);
    }

    const uint64_t esr = reinterpret_cast<esr_context *>(ctx)->esr;
#endif
    // https://developer.arm.com/documentation/ddi0595/2021-03/AArch64-Registers/ESR-EL1--Exception-Syndrome-Register--EL1-
    const uint32_t exception_class = static_cast<uint32_t>(esr) >> 26;
    const bool is_executing = (exception_class == 0b100000) || (exception_class == 0b100001);
    const bool is_data_abort = (exception_class == 0b100100) || (exception_class == 0b100101);
    const bool is_writing = is_data_abort && (esr & (1 << 6));
#else
#ifdef __APPLE__
    const uint64_t err = context->uc_mcontext->__es.__err;
#else
    const uint64_t err = context->uc_mcontext.gregs[REG_ERR];
#endif
    const bool is_executing = err & 0x10;
    const bool is_writing = err & 0x2;
#endif

    if (!is_executing) {
        if (access_violation_handler(reinterpret_cast<uint8_t *>(info->si_addr), is_writing)) {
            return;
        }
    }

    LOG_CRITICAL("Unhandled access to 0x{:X}", reinterpret_cast<uintptr_t>(info->si_addr));
    raise(SIGTRAP);
    return;
}

static void register_access_violation_handler(const AccessViolationHandler &handler) {
    access_violation_handler = handler;
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = signal_handler;
    if (sigaction(SIGSEGV, &sa, nullptr) == -1) {
        LOG_CRITICAL("Failed to register an exception handler");
    }
#ifdef __APPLE__
    // When accessing memory region which is PROT_NONE on macOS, it is raising SIGBUS not SIGSEGV.
    // So apply same signal handler to SIGBUS
    if (sigaction(SIGBUS, &sa, nullptr) == -1) {
        LOG_CRITICAL("Failed to register an exception handler to SIGBUS");
    }
#endif
}

#endif

static MemAllocationToken allocation_for_range(const MemState &state, Address address, uint64_t length) {
    if (!address || !length || length > UINT32_MAX || uint64_t(address) + length > UINT32_MAX)
        return {};
    auto allocation = state.allocation_generations.upper_bound(address);
    if (allocation == state.allocation_generations.begin())
        return {};
    --allocation;
    const uint32_t size = state.alloc_table[allocation->first / STANDARD_PAGE_SIZE].size * STANDARD_PAGE_SIZE;
    if (uint64_t(address) + length > uint64_t(allocation->first) + size)
        return {};
    return { allocation->first, size, allocation->second };
}

MemAllocationToken capture_mem_allocation(MemState &state, Address address, uint64_t length) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    return allocation_for_range(state, address, length);
}

bool matches_mem_allocation(MemState &state, const MemAllocationToken &expected, Address address, uint64_t length) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    return expected && allocation_for_range(state, address, length) == expected;
}

// Called only with protect_mutex held. Never waits for CPU or renderer work.
static MemPerm readback_page_permission(const MemState &state, uint64_t page) {
    MemPerm permission = MemPerm::ReadWrite;
    const uint64_t end = page + state.host_page_size;
    for (auto it = state.protect_tree.lower_bound(Address(end - 1)); it != state.protect_tree.end(); ++it) {
        if (uint64_t(it->first) + it->second.size <= page)
            break;
        permission = most_restrictive_perm(permission, it->second.perm);
    }
    for (const auto &registration : state.readback_watches)
        if (page < registration.end && end > registration.begin)
            if (auto watch = registration.watch.lock(); watch && !watch->invalidated)
                permission = MemPerm::None;
    return permission;
}

static void prune_readbacks_locked(MemState &state) {
    for (auto it = state.readback_watches.begin(); it != state.readback_watches.end();) {
        const auto watch = it->watch.lock();
        if (watch && !watch->invalidated) {
            ++it;
            continue;
        }
        const auto begin = it->begin, end = it->end;
        it = state.readback_watches.erase(it);
        for (uint64_t page = begin; page < end; page += state.host_page_size) {
            if (!protect_checked(state, Address(page), state.host_page_size, readback_page_permission(state, page))) {
                // The guest permissions are unknowable; do not resume a fault
                // or continue emulation with a possibly weakened shared page.
                LOG_CRITICAL("Could not restore readback page permissions");
                std::terminate();
            }
        }
    }
}

static void cancel_readbacks_locked(MemState &state, uint64_t begin, uint64_t end) {
    for (const auto &registration : state.readback_watches)
        if (begin < registration.end && end > registration.begin)
            if (auto watch = registration.watch.lock())
                watch->invalidated = true;
    prune_readbacks_locked(state);
}

bool gpu_readback_supported() {
#ifdef __APPLE__
    return true;
#else
    // A protected guest view must never be temporarily opened for readback.
    // Add another backend only after its same-backing alias contract is tested.
    return false;
#endif
}

std::shared_ptr<GpuReadbackWatch> begin_gpu_readback_watch(MemState &state,
    const MemAllocationToken &allocation, Address destination,
    uint32_t row_bytes, uint32_t height, uint32_t stride) {
    if (!gpu_readback_supported() || !destination || !row_bytes || !height || row_bytes > stride)
        return {};
    const uint64_t length = uint64_t(height - 1) * stride + row_bytes;
    if (length > 64 * 1024 * 1024 || uint64_t(destination) + length > UINT32_MAX)
        return {};
    const std::lock_guard<std::mutex> generation_lock(state.generation_mutex);
    const std::lock_guard<std::mutex> protection_lock(state.protect_mutex);
    if (state.use_page_table || !allocation || allocation_for_range(state, destination, length) != allocation)
        return {};
    prune_readbacks_locked(state);
    auto watch = std::make_shared<GpuReadbackWatch>();
    watch->allocation = allocation;
    watch->destination = destination;
    watch->row_bytes = row_bytes;
    watch->height = height;
    watch->stride = stride;
    const uint64_t begin = align_down(uint64_t(destination), uint64_t(state.host_page_size));
    const uint64_t end = align(uint64_t(destination) + length, uint64_t(state.host_page_size));
    state.readback_watches.push_back({ begin, end, watch });
    if (!protect_checked(state, Address(begin), uint32_t(end - begin), MemPerm::None)) {
        watch->invalidated = true;
        prune_readbacks_locked(state);
        return {};
    }
    return watch;
}

void finish_gpu_readback_watch(MemState &state, const std::shared_ptr<GpuReadbackWatch> &watch) {
    if (!watch)
        return;
    const std::lock_guard<std::mutex> generation_lock(state.generation_mutex);
    const std::lock_guard<std::mutex> protection_lock(state.protect_mutex);
    watch->invalidated = true;
    prune_readbacks_locked(state);
}

bool write_gpu_readback_rows(MemState &state, Address destination, const uint8_t *source,
    uint32_t row_bytes, uint32_t height, uint32_t destination_stride,
    const MemAllocationToken *expected, const GpuReadbackWatch *watch) {
    if (!gpu_readback_supported() || !source || !destination || !row_bytes || !height || row_bytes > destination_stride)
        return false;
    const uint64_t length = uint64_t(height - 1) * destination_stride + row_bytes;
    const uint64_t end = uint64_t(destination) + length;
    if (length > 64 * 1024 * 1024 || end > UINT32_MAX)
        return false;
    const std::lock_guard<std::mutex> generation_lock(state.generation_mutex);
    const std::lock_guard<std::mutex> protection_lock(state.protect_mutex);
    if (state.use_page_table || !is_valid_addr_range(state, destination, uint32_t(end)))
        return false;
    if (expected && (!*expected || allocation_for_range(state, destination, length) != *expected))
        return false;
    if (watch && (watch->invalidated || !expected || watch->allocation != *expected || watch->destination != destination || watch->row_bytes != row_bytes || watch->height != height || watch->stride != destination_stride))
        return false;
#ifdef __APPLE__
    const uint64_t begin_page = align_down(uint64_t(destination), uint64_t(state.host_page_size));
    const uint64_t alias_size = align(end, uint64_t(state.host_page_size)) - begin_page;
    mach_vm_address_t alias = 0;
    vm_prot_t current = 0, maximum = 0;
    if (mach_vm_remap(mach_task_self(), &alias, alias_size, 0, VM_FLAGS_ANYWHERE,
            mach_task_self(), reinterpret_cast<mach_vm_address_t>(state.memory.get() + begin_page),
            false, &current, &maximum, VM_INHERIT_NONE)
        != KERN_SUCCESS)
        return false;
    struct AliasLease {
        mach_vm_address_t address;
        mach_vm_size_t size;
        ~AliasLease() { mach_vm_deallocate(mach_task_self(), address, size); }
    } lease{ alias, alias_size };
    if (!(maximum & VM_PROT_WRITE)
        || mach_vm_protect(mach_task_self(), alias, alias_size, false, VM_PROT_READ | VM_PROT_WRITE) != KERN_SUCCESS)
        return false;
    uint8_t *output = reinterpret_cast<uint8_t *>(alias) + (destination - begin_page);
    for (uint32_t y = 0; y < height; ++y)
        std::memcpy(output + uint64_t(y) * destination_stride, source + uint64_t(y) * row_bytes, row_bytes);
    return true;
#else
    return false;
#endif
}
