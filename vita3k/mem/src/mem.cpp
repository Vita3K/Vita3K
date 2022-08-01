// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <cmath>
#include <cstring>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

constexpr size_t STANDARD_PAGE_SIZE = 4096;
constexpr size_t TOTAL_MEM_SIZE = GiB(4);
constexpr bool LOG_PROTECT = false;
constexpr bool PAGE_NAME_TRACKING = false;

// TODO: support multiple handlers
static AccessViolationHandler access_violation_handler;
static void register_access_violation_handler(AccessViolationHandler handler);

static Address alloc_inner(MemState &state, uint32_t start_page, int page_count, const char *name, const bool force);
static void delete_memory(uint8_t *memory);
static void delete_pagetable(MemPage *page_table);

bool init(MemState &state) {
#ifdef WIN32
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    state.page_size = system_info.dwPageSize;
#else
    state.page_size = sysconf(_SC_PAGESIZE);
#endif
    state.page_size = std::max(STANDARD_PAGE_SIZE, state.page_size);

    assert(state.page_size >= 4096); // Limit imposed by Unicorn.

    void *preferred_address = reinterpret_cast<void *>(1ULL << 34);

#ifdef WIN32
    state.memory = Memory(static_cast<uint8_t *>(VirtualAlloc(preferred_address, TOTAL_MEM_SIZE, MEM_RESERVE, PAGE_NOACCESS)), delete_memory);
    if (!state.memory) {
        // fallback
        state.memory = Memory(static_cast<uint8_t *>(VirtualAlloc(nullptr, TOTAL_MEM_SIZE, MEM_RESERVE, PAGE_NOACCESS)), delete_memory);

        if (!state.memory) {
            LOG_CRITICAL("VirtualAlloc failed: {}", log_hex(GetLastError()));
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
        LOG_CRITICAL("mmap failed");
        return false;
    }
#endif

    const size_t table_length = TOTAL_MEM_SIZE / state.page_size;
    state.page_table = PageTable(new MemPage[table_length], delete_pagetable);
    memset(state.page_table.get(), 0, sizeof(MemPage) * table_length);

    state.allocator.set_maximum(table_length);

    const auto handler = [&state](uint8_t *addr, bool write) noexcept {
        return handle_access_violation(state, addr, write);
    };
    register_access_violation_handler(handler);

    const Address null_address = alloc_inner(state, 0, 1, "null", true);
    assert(null_address == 0);
#ifdef WIN32
    DWORD old_protect = 0;
    const BOOL ret = VirtualProtect(state.memory.get(), state.page_size, PAGE_NOACCESS, &old_protect);
    LOG_CRITICAL_IF(!ret, "VirtualAlloc failed: {}", log_hex(GetLastError()));
#else
    mprotect(state.memory.get(), state.page_size, PROT_NONE);
#endif

    return true;
}

static void delete_memory(uint8_t *memory) {
    if (memory != nullptr) {
#ifdef WIN32
        const BOOL ret = VirtualFree(memory, 0, MEM_RELEASE);
        assert(ret);
#else
        munmap(memory, TOTAL_MEM_SIZE);
#endif
    }
}

static void delete_pagetable(MemPage *page_table) {
    delete[] page_table;
}

bool is_valid_addr(const MemState &state, Address addr) {
    const size_t page_num = addr / state.page_size;
    return addr && state.allocator.free_slot_count(page_num, page_num + 1) == 0;
}

bool is_valid_addr_range(const MemState &state, Address start, Address end) {
    const uint32_t start_page = start / state.page_size;
    const uint32_t end_page = (end + state.page_size - 1) / state.page_size;
    return state.allocator.free_slot_count(start_page, end_page) == 0;
}

static Address alloc_inner(MemState &state, uint32_t start_page, int page_count, const char *name, const bool force) {
    int page_num;
    if (force) {
        if (state.allocator.allocate_at(start_page, page_count) < 0) {
            LOG_CRITICAL("Failed to allocate at specific page");
        }
        page_num = start_page;
    } else {
        page_num = state.allocator.allocate_from(start_page, page_count, false);
        if (page_num < 0)
            return 0;
    }

    const int size = page_count * state.page_size;
    const Address addr = page_num * state.page_size;
    uint8_t *const memory = &state.memory[addr];

    // Make memory chunck available to access
#ifdef WIN32
    const void *const ret = VirtualAlloc(memory, size, MEM_COMMIT, PAGE_READWRITE);
    LOG_CRITICAL_IF(!ret, "VirtualAlloc failed: {}", log_hex(GetLastError()));
#else
    mprotect(memory, size, PROT_READ | PROT_WRITE);
#endif
    std::memset(memory, 0, size);

    MemPage &page = state.page_table[page_num];
    assert(!page.allocated);
    page.allocated = 1;
    page.size = page_count;

    if (PAGE_NAME_TRACKING) {
        state.page_name_map.emplace(page_num, name);
    }

    return addr;
}

Address alloc(MemState &state, size_t size, const char *name, unsigned int alignment) {
    if (alignment == 0)
        return alloc(state, size, name);
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    size += alignment;
    const size_t page_count = align(size, state.page_size) / state.page_size;
    const Address addr = alloc_inner(state, 0, page_count, name, false);
    const Address align_addr = align(addr, alignment);
    const size_t page_num = addr / state.page_size;
    const size_t align_page_num = align_addr / state.page_size;

    if (page_num != align_page_num) {
        MemPage &page = state.page_table[page_num];
        MemPage &align_page = state.page_table[align_page_num];
        const size_t remnant_front = align_page_num - page_num;
        state.allocator.free(page_num, remnant_front);
        page.allocated = 0;
        align_page.allocated = 1;
        align_page.size = page.size - remnant_front;
    }

    return align_addr;
}

static void align_to_page(MemState &state, ProtectSegmentInfo &protect) {
    const Address end = align(protect.addr + protect.size, state.page_size);
    const Address addr = align_down(protect.addr, state.page_size);
    const size_t size = end - addr;
    protect.addr = addr;
    protect.size = size;
}

static bool overlap_in_page(MemState &state, const ProtectSegmentInfo &a, const ProtectSegmentInfo &b) {
    const Address a_start = align_down(a.addr, state.page_size);
    const Address a_end = align(a.addr + a.size, state.page_size);
    const Address b_start = align_down(b.addr, state.page_size);
    const Address b_end = align(b.addr + b.size, state.page_size);
    return a_start <= b_end && b_start <= a_end;
}

void unprotect_inner(MemState &state, Address addr, size_t size) {
    if (LOG_PROTECT) {
        fmt::print("Unprotect: {} {}\n", log_hex(addr), size);
    }
#ifdef WIN32
    DWORD old_protect = 0;
    const BOOL ret = VirtualProtect(&state.memory[addr], size - 1, PAGE_READWRITE, &old_protect);
    LOG_CRITICAL_IF(!ret, "VirtualAlloc failed: {}", log_hex(GetLastError()));
#else
    mprotect(&state.memory[addr], size, PROT_READ | PROT_WRITE);
#endif
}

void protect_inner(MemState &state, Address addr, size_t size, const std::uint32_t perm) {
#ifdef WIN32
    DWORD old_protect = 0;
    const BOOL ret = VirtualProtect(&state.memory[addr], size - 1, (perm == MEM_PERM_NONE) ? PAGE_NOACCESS : ((perm == MEM_PERM_READONLY) ? PAGE_READONLY : PAGE_READWRITE), &old_protect);
    LOG_CRITICAL_IF(!ret, "VirtualAlloc failed: {}", log_hex(GetLastError()));

#else
    mprotect(&state.memory[addr], size, (perm == MEM_PERM_NONE) ? PROT_NONE : ((perm == MEM_PERM_READONLY) ? PROT_READ : (PROT_READ | PROT_WRITE)));
#endif
}

static ProtectSegmentTrees::iterator find_protect_segment(ProtectSegmentTrees &tree, Address addr) {
    if (tree.empty()) {
        return tree.end();
    }
    return --tree.upper_bound(ProtectSegmentInfo(addr));
}

bool handle_access_violation(MemState &state, uint8_t *addr, bool write) noexcept {
    const uintptr_t memory_addr = reinterpret_cast<uintptr_t>(state.memory.get());
    const uintptr_t fault_addr = reinterpret_cast<uintptr_t>(addr);
    if (fault_addr < memory_addr || fault_addr >= memory_addr + TOTAL_MEM_SIZE) {
        return false;
    }
    const Address vaddr = fault_addr - memory_addr;
    if (!is_valid_addr(state, vaddr)) {
        return false;
    }
    if (LOG_PROTECT) {
        fmt::print("Access: {}\n", log_hex(vaddr));
    }

    const std::unique_lock<std::mutex> lock(state.protect_mutex);
    const auto it = find_protect_segment(state.protect_tree, vaddr);
    if (it == state.protect_tree.end()) {
        // HACK: keep going
        unprotect_inner(state, vaddr, 4);
        LOG_CRITICAL("Unhandled write protected region was valid. Address=0x{:X}", vaddr);
        return true;
    }

    if (vaddr < it->addr || vaddr >= it->addr + it->size) {
        // HACK: keep going
        unprotect_inner(state, vaddr, 4);
        LOG_CRITICAL("Unhandled write protected region was valid. Address=0x{:X}", vaddr);
        return true;
    }

    Address previous_beg = it->addr;

    for (auto ite = it->blocks.begin(); ite != it->blocks.end();) {
        if (ite->callback(vaddr, write)) {
            Address beg_unpr = align_down(ite->addr, state.page_size);
            Address end_unpr = align(ite->addr + ite->size, state.page_size);
            unprotect_inner(state, beg_unpr, end_unpr - beg_unpr);

            ite = const_cast<std::set<ProtectBlockInfo> &>(it->blocks).erase(ite);
        } else {
            ite++;
        }
    }

    if ((it->blocks.size() == 0) && (it->ref_count == 0)) {
        unprotect_inner(state, it->addr, it->size);
        state.protect_tree.erase(it);
    } else {
        Address beg_region = it->blocks.begin()->addr;
        Address end_region = it->blocks.rbegin()->addr + it->blocks.rbegin()->size;

        beg_region = align_down(beg_region, state.page_size);
        end_region = align(end_region, state.page_size);

        if (beg_region != previous_beg) {
            ProtectSegmentInfo info = std::move(*it);
            info.addr = beg_region;
            info.size = end_region - beg_region;

            state.protect_tree.erase(it);
            state.protect_tree.emplace(std::move(info));
        } else {
            const_cast<std::size_t &>(it->size) = end_region - beg_region;
        }
    }

    return true;
}

bool add_protect(MemState &state, Address addr, const size_t size, const std::uint32_t perm, ProtectCallback callback) {
    const std::lock_guard<std::mutex> lock(state.protect_mutex);
    ProtectSegmentInfo protect(addr, size, perm);
    align_to_page(state, protect);

    ProtectBlockInfo block;
    block.addr = addr;
    block.size = size;
    block.callback = callback;

    protect.blocks.emplace(block);

    auto it = find_protect_segment(state.protect_tree, protect.addr);
    while (it != state.protect_tree.end() && overlap_in_page(state, *it, protect)) {
        const Address start = std::min(it->addr, protect.addr);
        protect.size = std::max(it->addr + it->size, protect.addr + protect.size) - start;
        protect.addr = start;
        protect.ref_count = it->ref_count; // Transfer access count to new block
        protect.blocks.insert(it->blocks.begin(), it->blocks.end());

        state.protect_tree.erase(it++);
    }

    if (protect.ref_count == 0) {
        protect_inner(state, protect.addr, protect.size, perm);
    }

    state.protect_tree.emplace(protect);
    return true;
}

bool is_protecting(MemState &state, Address addr, std::uint32_t *perm) {
    const std::lock_guard<std::mutex> lock(state.protect_mutex);
    auto ite = find_protect_segment(state.protect_tree, addr);

    if ((ite != state.protect_tree.end()) && (addr >= ite->addr) && (addr < ite->addr + ite->size)) {
        if (perm) {
            *perm = ite->perm;
        }

        return true;
    }

    return false;
}

void open_access_parent_protect_segment(MemState &state, Address addr) {
    const std::lock_guard<std::mutex> lock(state.protect_mutex);
    auto ite = find_protect_segment(state.protect_tree, addr);

    if ((ite != state.protect_tree.end()) && (addr >= ite->addr) && (addr < ite->addr + ite->size)) {
        const_cast<std::int32_t &>(ite->ref_count)++;
    } else {
        ProtectSegmentInfo protect(align_down(addr, state.page_size), 0, 0);
        protect.ref_count = 1;

        state.protect_tree.emplace(protect);
    }
}

void close_access_parent_protect_segment(MemState &state, Address addr) {
    const std::lock_guard<std::mutex> lock(state.protect_mutex);
    auto ite = find_protect_segment(state.protect_tree, addr);

    if (ite != state.protect_tree.end()) {
        if (ite->ref_count > 0) {
            const_cast<std::int32_t &>(ite->ref_count)--;
        }

        if (ite->ref_count == 0) {
            if ((ite->blocks.size() == 0) || (ite->size == 0)) {
                state.protect_tree.erase(ite);
            } else {
                protect_inner(state, ite->addr, ite->size, ite->perm);
            }
        }
    }
}

Address alloc(MemState &state, size_t size, const char *name) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    const size_t page_count = align(size, state.page_size) / state.page_size;
    const Address addr = alloc_inner(state, 0, page_count, name, false);
    return addr;
}

Address alloc_at(MemState &state, Address address, size_t size, const char *name) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    const uint32_t wanted_page = address / state.page_size;
    size += address % state.page_size;
    const size_t page_count = align(size, state.page_size) / state.page_size;
    alloc_inner(state, wanted_page, page_count, name, true);
    return address;
}

Block alloc_block(MemState &mem, size_t size, const char *name) {
    const Address address = alloc(mem, size, name);
    return Block(address, [&mem](Address stack) {
        free(mem, stack);
    });
}

void free(MemState &state, Address address) {
    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    const size_t page_num = address / state.page_size;
    assert(page_num >= 0);

    MemPage &page = state.page_table[page_num];
    if (!page.allocated) {
        LOG_CRITICAL("Freeing unallocated page");
    }
    page.allocated = 0;

    state.allocator.free(page_num, page.size);
    if (PAGE_NAME_TRACKING) {
        state.page_name_map.erase(page_num);
    }

    uint8_t *const memory = &state.memory[page_num * state.page_size];

#ifdef WIN32
    const BOOL ret = VirtualFree(memory, page.size * state.page_size, MEM_DECOMMIT);
    assert(ret);
#else
    mprotect(memory, page.size * state.page_size, PROT_NONE);
#endif
}

uint32_t mem_available(MemState &state) {
    return state.allocator.free_slot_count(0, state.allocator.max_offset) * state.page_size;
}

const char *mem_name(Address address, MemState &state) {
    if (PAGE_NAME_TRACKING) {
        return state.page_name_map.find(address / state.page_size)->second.c_str();
    }
    return "";
}

#ifdef WIN32

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

static void register_access_violation_handler(AccessViolationHandler handler) {
    access_violation_handler = handler;
    if (!AddVectoredExceptionHandler(1, (PVECTORED_EXCEPTION_HANDLER)exception_handler)) {
        LOG_CRITICAL("Failed to register an exception handler");
    }
}

#else

static void signal_handler(int sig, siginfo_t *info, void *uct) noexcept {
    auto context = static_cast<ucontext_t *>(uct);

#ifdef __APPLE__
    const uint64_t err = context->uc_mcontext->__es.__err;
#else
    const uint64_t err = context->uc_mcontext.gregs[REG_ERR];
#endif

    const bool is_executing = err & 0x10;
    const bool is_writing = err & 0x2;

    if (!is_executing) {
        if (access_violation_handler(reinterpret_cast<uint8_t *>(info->si_addr), is_writing)) {
            return;
        }
    }

    raise(SIGTRAP);
    return;
}

static void register_access_violation_handler(AccessViolationHandler handler) {
    access_violation_handler = handler;
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = signal_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        LOG_CRITICAL("Failed to register an exception handler");
    }
}

#endif
