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
#include <mem/ptr.h>
#include <mem/state.h>

#include <util/align.h>
#include <util/log.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <csignal>
#include <sys/mman.h>
#include <unistd.h>
#endif

constexpr uint32_t STANDARD_PAGE_SIZE = KiB(4);
constexpr size_t TOTAL_MEM_SIZE = GiB(4);
constexpr bool LOG_PROTECT = false;
#ifdef NDEBUG
constexpr bool PAGE_NAME_TRACKING = false;
#else
constexpr bool PAGE_NAME_TRACKING = true;
#endif

struct FaultMapping {
    FaultMapping *next = nullptr;
    std::atomic<bool> active = false;
    std::atomic<uint32_t> readers = 0;
    bool available = true;
    uintptr_t begin = 0;
    uintptr_t end = 0;
};

struct FaultMemory {
    FaultMemory *next = nullptr;
    std::atomic<MemState *> state = nullptr;
    std::atomic<uint32_t> readers = 0;
    bool available = true;
    std::atomic<FaultMapping *> mappings = nullptr;
    std::vector<std::unique_ptr<FaultMapping>> owned_mappings;
};

static_assert(std::atomic<MemState *>::is_always_lock_free);
static_assert(std::atomic<FaultMemory *>::is_always_lock_free);
static_assert(std::atomic<FaultMapping *>::is_always_lock_free);
static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);

static std::mutex fault_registry_mutex;
static std::atomic<FaultMemory *> fault_memories = nullptr;
static std::atomic<uint32_t> active_fault_dispatches = 0;

struct FaultRegistry {
    std::vector<std::unique_ptr<FaultMemory>> entries;

    ~FaultRegistry() {
        fault_memories.store(nullptr);
        while (active_fault_dispatches.load() != 0)
            std::this_thread::yield();
    }
};

// Published nodes remain allocated until shutdown and are reused only after readers drain.
static FaultRegistry fault_registry;
static std::once_flag fault_handler_once;
static bool fault_handler_installed = false;

static bool install_access_violation_handler();

static bool register_fault_memory(MemState &state) {
    std::call_once(fault_handler_once, [] { fault_handler_installed = install_access_violation_handler(); });
    if (!fault_handler_installed)
        return false;

    const std::lock_guard lock(fault_registry_mutex);
    FaultMemory *entry = nullptr;
    for (const auto &candidate : fault_registry.entries) {
        if (candidate->available) {
            entry = candidate.get();
            break;
        }
    }
    if (!entry) {
        auto candidate = std::make_unique<FaultMemory>();
        entry = candidate.get();
        entry->next = fault_memories.load();
        fault_registry.entries.push_back(std::move(candidate));
        fault_memories.store(entry);
    }
    entry->available = false;
    entry->state.store(&state);
    return true;
}

static void unregister_fault_memory(MemState &state) {
    FaultMemory *entry = nullptr;
    {
        const std::lock_guard lock(fault_registry_mutex);
        for (const auto &candidate : fault_registry.entries) {
            if (candidate->state.load() == &state) {
                entry = candidate.get();
                entry->state.store(nullptr);
                break;
            }
        }
    }
    if (!entry)
        return;

    while (entry->readers.load() != 0)
        std::this_thread::yield();

    const std::lock_guard lock(fault_registry_mutex);
    for (const auto &mapping : entry->owned_mappings) {
        mapping->active.store(false);
        mapping->available = true;
    }
    entry->available = true;
}

static FaultMemory *find_fault_memory_locked(MemState &state) {
    for (const auto &entry : fault_registry.entries) {
        if (entry->state.load() == &state)
            return entry.get();
    }
    return nullptr;
}

static void register_fault_mapping(MemState &state, uint8_t *address, uint32_t size) {
    const std::lock_guard lock(fault_registry_mutex);
    FaultMemory *entry = find_fault_memory_locked(state);
    assert(entry);
    if (!entry) {
        LOG_CRITICAL("Cannot register an external mapping for an unregistered memory state");
        return;
    }
    FaultMapping *mapping = nullptr;
    for (const auto &candidate : entry->owned_mappings) {
        if (candidate->available) {
            mapping = candidate.get();
            break;
        }
    }
    if (!mapping) {
        auto candidate = std::make_unique<FaultMapping>();
        mapping = candidate.get();
        mapping->next = entry->mappings.load();
        entry->owned_mappings.push_back(std::move(candidate));
        entry->mappings.store(mapping);
    }
    mapping->available = false;
    mapping->begin = reinterpret_cast<uintptr_t>(address);
    mapping->end = mapping->begin + size;
    mapping->active.store(true);
}

static void unregister_fault_mapping(MemState &state, uint8_t *address) {
    FaultMapping *mapping = nullptr;
    {
        const std::lock_guard lock(fault_registry_mutex);
        FaultMemory *entry = find_fault_memory_locked(state);
        if (entry) {
            for (const auto &candidate : entry->owned_mappings) {
                if (candidate->active.load() && candidate->begin == reinterpret_cast<uintptr_t>(address)) {
                    mapping = candidate.get();
                    mapping->active.store(false);
                    break;
                }
            }
        }
    }
    assert(mapping);
    if (!mapping) {
        LOG_CRITICAL("Cannot unregister an unknown external mapping");
        return;
    }
    while (mapping->readers.load() != 0)
        std::this_thread::yield();
    const std::lock_guard lock(fault_registry_mutex);
    mapping->available = true;
}

static bool dispatch_access_violation(uint8_t *address, bool write) noexcept {
    struct DispatchGuard {
        DispatchGuard() { active_fault_dispatches.fetch_add(1); }
        ~DispatchGuard() { active_fault_dispatches.fetch_sub(1); }
    } guard;
    const uintptr_t fault = reinterpret_cast<uintptr_t>(address);
    for (FaultMemory *entry = fault_memories.load(); entry; entry = entry->next) {
        MemState *state = entry->state.load();
        if (!state)
            continue;
        // Pin the entry before dereferencing state; teardown waits for pinned handlers.
        entry->readers.fetch_add(1);
        if (entry->state.load() != state) {
            entry->readers.fetch_sub(1);
            continue;
        }

        const uintptr_t begin = reinterpret_cast<uintptr_t>(state->memory.get());
        bool matches = fault >= begin && fault - begin < TOTAL_MEM_SIZE;
        bool handled = matches && handle_access_violation(*state, address, write);
        if (!matches) {
            for (FaultMapping *mapping = entry->mappings.load(); mapping; mapping = mapping->next) {
                if (!mapping->active.load())
                    continue;
                mapping->readers.fetch_add(1);
                if (mapping->active.load()) {
                    matches = fault >= mapping->begin && fault < mapping->end;
                    if (matches)
                        handled = handle_access_violation(*state, address, write);
                }
                mapping->readers.fetch_sub(1);
                if (matches)
                    break;
            }
        }
        entry->readers.fetch_sub(1);
        if (handled)
            return true;
    }
    return false;
}

static Address alloc_inner(MemState &state, uint32_t start_page, uint32_t page_count, const char *name, const bool force);
static void delete_memory(uint8_t *memory);

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

    if (!register_fault_memory(state)) {
        deinit_mem(state);
        return false;
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

// Whether the len bytes at addr are all mapped
bool is_valid_addr_len(const MemState &state, Address addr, uint32_t len) {
    const uint64_t end = static_cast<uint64_t>(addr) + len;
    if (end > (UINT64_C(1) << 32))
        return false;
    const uint32_t start_page = addr / STANDARD_PAGE_SIZE;
    const uint32_t end_page = static_cast<uint32_t>((end + STANDARD_PAGE_SIZE - 1) / STANDARD_PAGE_SIZE);
    return state.allocator.free_slot_count(start_page, end_page) == 0;
}

uint64_t pin_guest_memory(MemState &state, std::span<const GuestMemoryRange> ranges) {
    const std::lock_guard lock(state.generation_mutex);
    if (!state.memory || !state.alloc_table)
        return 0;
    for (const GuestMemoryRange &range : ranges) {
        if (range.size == 0)
            continue;
        const uint64_t end = uint64_t(range.address) + range.size;
        if (range.address < STANDARD_PAGE_SIZE || end > TOTAL_MEM_SIZE)
            return 0;
        const uint32_t first = range.address / STANDARD_PAGE_SIZE;
        const uint32_t last = static_cast<uint32_t>((end + STANDARD_PAGE_SIZE - 1) / STANDARD_PAGE_SIZE);
        if (state.allocator.free_slot_count(first, last) != 0)
            return 0;
    }
    if (state.next_pin == 0)
        return 0;
    const uint64_t pin = state.next_pin++;
    state.pinned_ranges.emplace(pin, std::vector<GuestMemoryRange>(ranges.begin(), ranges.end()));
    return pin;
}

void unpin_guest_memory(MemState &state, uint64_t pin) {
    {
        const std::lock_guard lock(state.generation_mutex);
        state.pinned_ranges.erase(pin);
    }
    state.pins_changed.notify_all();
}

bool copy_guest_memory(MemState &state, Address destination, Address source, uint32_t size) {
    if (!size)
        return true;
    const uint64_t source_end = uint64_t(source) + size;
    const uint64_t destination_end = uint64_t(destination) + size;
    if (!source || !destination || (source < destination_end && destination < source_end))
        return false;
    const std::array<GuestMemoryRange, 2> ranges{ GuestMemoryRange{ source, size }, GuestMemoryRange{ destination, size } };
    const uint64_t pin = pin_guest_memory(state, ranges);
    if (!pin)
        return false;
    uint32_t offset = 0;
    while (offset < size) {
        const Address from = source + offset;
        const Address to = destination + offset;
        const uint32_t chunk = std::min({ size - offset, STANDARD_PAGE_SIZE - (from & (STANDARD_PAGE_SIZE - 1)),
            STANDARD_PAGE_SIZE - (to & (STANDARD_PAGE_SIZE - 1)) });
        std::memcpy(Ptr<uint8_t>(to).get(state), Ptr<const uint8_t>(from).get(state), chunk);
        offset += chunk;
    }
    unpin_guest_memory(state, pin);
    return true;
}

bool move_guest_memory(MemState &state, Address destination, Address source, uint32_t size) {
    if (!size)
        return true;
    const std::array<GuestMemoryRange, 2> ranges{ GuestMemoryRange{ source, size }, GuestMemoryRange{ destination, size } };
    const uint64_t pin = pin_guest_memory(state, ranges);
    if (!pin)
        return false;
    if (destination > source && uint64_t(destination) < uint64_t(source) + size) {
        uint32_t remaining = size;
        while (remaining) {
            const Address from = source + remaining - 1;
            const Address to = destination + remaining - 1;
            const uint32_t chunk = std::min({ remaining, (from & (STANDARD_PAGE_SIZE - 1)) + 1,
                (to & (STANDARD_PAGE_SIZE - 1)) + 1 });
            std::memmove(Ptr<uint8_t>(to - chunk + 1).get(state), Ptr<const uint8_t>(from - chunk + 1).get(state), chunk);
            remaining -= chunk;
        }
    } else {
        uint32_t offset = 0;
        while (offset < size) {
            const Address from = source + offset;
            const Address to = destination + offset;
            const uint32_t chunk = std::min({ size - offset, STANDARD_PAGE_SIZE - (from & (STANDARD_PAGE_SIZE - 1)),
                STANDARD_PAGE_SIZE - (to & (STANDARD_PAGE_SIZE - 1)) });
            std::memmove(Ptr<uint8_t>(to).get(state), Ptr<const uint8_t>(from).get(state), chunk);
            offset += chunk;
        }
    }
    unpin_guest_memory(state, pin);
    return true;
}

bool fill_guest_memory(MemState &state, Address destination, uint8_t value, uint32_t size) {
    if (!size)
        return true;
    const GuestMemoryRange range{ destination, size };
    const uint64_t pin = pin_guest_memory(state, std::span(&range, 1));
    if (!pin)
        return false;
    uint32_t offset = 0;
    while (offset < size) {
        const Address to = destination + offset;
        const uint32_t chunk = std::min(size - offset, STANDARD_PAGE_SIZE - (to & (STANDARD_PAGE_SIZE - 1)));
        std::memset(Ptr<uint8_t>(to).get(state), value, chunk);
        offset += chunk;
    }
    unpin_guest_memory(state, pin);
    return true;
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

    const Address commit_start = align_down(addr, state.host_page_size);
    const Address commit_end = align(addr + size, state.host_page_size);
    const uint32_t commit_size = commit_end - commit_start;
    uint8_t *const commit_ptr = &state.memory[commit_start];

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
    }

    return align_addr;
}

static void align_to_page(MemState &state, Address &addr, Address &size) {
    const Address end = align(addr + size, STANDARD_PAGE_SIZE);
    addr = align_down(addr, STANDARD_PAGE_SIZE);
    size = end - addr;
}

void unprotect_inner(MemState &state, Address addr, uint32_t size) {
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
    LOG_CRITICAL_IF(!ret, "VirtualProtect failed while unprotecting guest range {}+{} (host range {}+{}): {}",
        log_hex(addr), log_hex(size), log_hex(reinterpret_cast<uintptr_t>(aligned_start)), log_hex(aligned_size), get_error_msg());
#else
    const int ret = mprotect(aligned_start, aligned_size, PROT_READ | PROT_WRITE);
    LOG_CRITICAL_IF(ret == -1, "mprotect failed: {}", get_error_msg());
#endif
}

void protect_inner(MemState &state, Address addr, uint32_t size, const MemPerm perm) {
    uint8_t *addr_ptr = state.use_page_table ? state.page_table[addr / KiB(4)] : state.memory.get();

    uint8_t *target = &addr_ptr[addr];
    uint8_t *aligned_start = reinterpret_cast<uint8_t *>(
        align_down(reinterpret_cast<uintptr_t>(target), state.host_page_size));
    uint8_t *aligned_end = reinterpret_cast<uint8_t *>(align(reinterpret_cast<uintptr_t>(target + size), state.host_page_size));
    size_t aligned_size = aligned_end - aligned_start;

#ifdef _WIN32
    DWORD old_protect = 0;
    const BOOL ret = VirtualProtect(aligned_start, aligned_size, (perm == MemPerm::None) ? PAGE_NOACCESS : ((perm == MemPerm::ReadOnly) ? PAGE_READONLY : PAGE_READWRITE), &old_protect);
    LOG_CRITICAL_IF(!ret, "VirtualProtect failed while protecting guest range {}+{} (host range {}+{}): {}",
        log_hex(addr), log_hex(size), log_hex(reinterpret_cast<uintptr_t>(aligned_start)), log_hex(aligned_size), get_error_msg());
#else
    const int ret = mprotect(aligned_start, aligned_size, (perm == MemPerm::None) ? PROT_NONE : ((perm == MemPerm::ReadOnly) ? PROT_READ : (PROT_READ | PROT_WRITE)));
    LOG_CRITICAL_IF(ret == -1, "mprotect failed: {}", get_error_msg());
#endif
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

    // Never recover accesses to the null guard page by silently unprotecting it.
    if (vaddr < STANDARD_PAGE_SIZE) {
        return false;
    }

    if (!is_valid_addr(state, vaddr)) {
        return false;
    }
    if (LOG_PROTECT) {
        fmt::print("Access: {}\n", log_hex(vaddr));
    }

    auto it = state.protect_tree.lower_bound(vaddr);
    if (it == state.protect_tree.end()) {
        // HACK: keep going
        unprotect_inner(state, align_down(vaddr, state.host_page_size), state.host_page_size);
        LOG_CRITICAL("Unhandled protected {} access to valid address=0x{:X}", write ? "write" : "read", vaddr);
        return true;
    }

    ProtectSegmentInfo &info = it->second;
    if (vaddr < it->first || vaddr >= it->first + info.size) {
        // HACK: keep going
        unprotect_inner(state, align_down(vaddr, state.host_page_size), state.host_page_size);
        LOG_CRITICAL("Unhandled protected {} access to valid address=0x{:X}", write ? "write" : "read", vaddr);
        return true;
    }

    Address previous_beg = it->first;
    for (auto &[block_addr, block] : info.blocks) {
        block.callback(vaddr, write);
    }

    unprotect_inner(state, it->first, info.size);
    state.protect_tree.erase(it);

    return true;
}

bool add_protect(MemState &state, Address addr, const uint32_t size, const MemPerm perm, const ProtectCallback &callback) {
    const std::lock_guard<std::mutex> lock(state.protect_mutex);
    ProtectSegmentInfo protect(size, perm);
    align_to_page(state, addr, protect.size);

    ProtectBlockInfo block;
    block.size = size;
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

    protect_inner(state, addr, protect.size, protect.perm);

    state.protect_tree.emplace(addr, std::move(protect));
    return true;
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
    register_fault_mapping(mem, addr_ptr, size);
}

void remove_external_mapping(MemState &mem, uint8_t *addr_ptr, uint32_t size) {
    uint64_t addr_value = std::bit_cast<uint64_t>(addr_ptr);
    MemExternalMapping mapping;
    if (mem.use_page_table) {
        unregister_fault_mapping(mem, addr_ptr);
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
        if (prot_it->first + prot_it->second.size <= mapping.address) {
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
    std::unique_lock<std::mutex> lock(state.generation_mutex);
    const uint32_t page_num = address / STANDARD_PAGE_SIZE;
    assert(page_num >= 0);

    AllocMemPage &page = state.alloc_table[page_num];
    if (!page.allocated) {
        LOG_CRITICAL("Freeing unallocated page");
    }
    const uint64_t allocation_start = uint64_t(page_num) * STANDARD_PAGE_SIZE;
    const uint64_t allocation_end = allocation_start + uint64_t(page.size) * STANDARD_PAGE_SIZE;
    state.pins_changed.wait(lock, [&] {
        for (const auto &[_, ranges] : state.pinned_ranges) {
            for (const GuestMemoryRange &range : ranges) {
                const uint64_t end = uint64_t(range.address) + range.size;
                if (range.size && range.address < allocation_end && end > allocation_start)
                    return false;
            }
        }
        return true;
    });
    page.allocated = 0;

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
    // Guest and renderer threads must stop using this state before memory is released.
    unregister_fault_memory(state);
    const std::lock_guard<std::mutex> gen_lock(state.generation_mutex);

    assert(state.pinned_ranges.empty());

    {
        const std::lock_guard<std::mutex> prot_lock(state.protect_mutex);
        state.protect_tree.clear();
    }

    state.memory.reset();
    state.alloc_table.reset();
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
        if (dispatch_access_violation(ptr, is_writing)) {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

static bool install_access_violation_handler() {
    if (!AddVectoredExceptionHandler(1, exception_handler)) {
        LOG_CRITICAL("Failed to register an exception handler");
        return false;
    }
    return true;
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
        if (dispatch_access_violation(reinterpret_cast<uint8_t *>(info->si_addr), is_writing)) {
            return;
        }
    }

    LOG_CRITICAL("Unhandled access to 0x{:X}", reinterpret_cast<uintptr_t>(info->si_addr));
    raise(SIGTRAP);
    return;
}

static bool install_access_violation_handler() {
    struct sigaction sa;
    struct sigaction previous_segv;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = signal_handler;
    if (sigaction(SIGSEGV, &sa, &previous_segv) == -1) {
        LOG_CRITICAL("Failed to register an exception handler");
        return false;
    }
#ifdef __APPLE__
    // When accessing memory region which is PROT_NONE on macOS, it is raising SIGBUS not SIGSEGV.
    // So apply same signal handler to SIGBUS
    if (sigaction(SIGBUS, &sa, nullptr) == -1) {
        LOG_CRITICAL("Failed to register an exception handler to SIGBUS");
        if (sigaction(SIGSEGV, &previous_segv, nullptr) == -1)
            LOG_CRITICAL("Failed to restore the previous SIGSEGV handler");
        return false;
    }
#endif
    return true;
}

#endif
