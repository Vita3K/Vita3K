// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <mem/mem.h>
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
#include <sys/mman.h>
#include <unistd.h>
#endif

constexpr size_t STANDARD_PAGE_SIZE = 4096;

static void delete_memory(uint8_t *memory) {
    if (memory != nullptr) {
#ifdef WIN32
        const BOOL ret = VirtualFree(memory, 0, MEM_RELEASE);
        assert(ret);
#else
        munmap(memory, GB(4));
#endif
    }
}

static void alloc_inner(MemState &state, Address address, size_t page_count, Allocated::iterator block, const char *name) {
    // If we chopped off bytes when aligning, they would have spilled over to another page, so add it here
    if (address % state.page_size != 0)
        page_count++;

    uint8_t *const memory = &state.memory[align_down(address, state.page_size)];

    const size_t size = page_count * state.page_size;

    const Generation generation = ++state.generation;
    std::fill_n(block, page_count, generation);

    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    state.generation_names[generation] = name;

#ifdef WIN32
    const void *const ret = VirtualAlloc(memory, size, MEM_COMMIT, PAGE_READWRITE);
    LOG_CRITICAL_IF(!ret, "VirtualAlloc failed: {}", log_hex(GetLastError()));
#else
    mprotect(memory, size, PROT_READ | PROT_WRITE);
#endif
    std::memset(memory, 0, size);
}

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

    const size_t length = GB(4);
#ifdef WIN32
    state.memory = Memory(static_cast<uint8_t *>(VirtualAlloc(nullptr, length, MEM_RESERVE, PAGE_NOACCESS)), delete_memory);
    if (!state.memory) {
        LOG_CRITICAL("VirtualAlloc failed: {}", log_hex(GetLastError()));
        return false;
    }
#else
    // http://man7.org/linux/man-pages/man2/mmap.2.html
    void *const addr = nullptr;
    const int prot = PROT_NONE;
    const int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    const int fd = 0;
    const off_t offset = 0;
    state.memory = Memory(static_cast<uint8_t *>(mmap(addr, length, prot, flags, fd, offset)), delete_memory);
    if (state.memory.get() == MAP_FAILED) {
        LOG_CRITICAL("mmap failed");
        return false;
    }
#endif

    state.allocated_pages.resize(length / state.page_size);
    const Address null_address = alloc(state, 1, "NULL");
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

Address alloc(MemState &state, size_t size, const char *name, unsigned int alignment) {
    if (alignment != 0)
        size += alignment - 1;
    auto addr = alloc(state, size, name);
    if (alignment == 0)
        return addr;
    auto new_addr = (addr + alignment - 1) & ~(alignment - 1);
    state.aligned_addr_to_original[new_addr] = addr;
    return new_addr;
}

Address alloc(MemState &state, size_t size, const char *name) {
    const size_t page_count = (size + (state.page_size - 1)) / state.page_size;
    const Allocated::iterator block = std::search_n(state.allocated_pages.begin(), state.allocated_pages.end(), page_count, 0);
    if (block == state.allocated_pages.end()) {
        assert(false);
        return 0;
    }

    const size_t block_page_index = block - state.allocated_pages.begin();
    const Address address = static_cast<Address>(block_page_index * state.page_size);

    alloc_inner(state, address, page_count, block, name);

    return address;
}

Address alloc_at(MemState &state, Address address, size_t size, const char *name) {
    const size_t page_count = (size + (state.page_size - 1)) / state.page_size;
    const Allocated::iterator block = state.allocated_pages.begin() + (address / state.page_size);

    alloc_inner(state, address, page_count, block, name);

    return address;
}

void free(MemState &state, Address address) {
    auto &addr_map = state.aligned_addr_to_original;
    if (addr_map.find(address) != addr_map.end()) {
        auto old_addr = address;
        address = addr_map[old_addr];
        addr_map.erase(old_addr);
    }
    const size_t page = address / state.page_size;
    assert(page >= 0);
    assert(page < state.allocated_pages.size());

    const Generation generation = state.allocated_pages[page];
    assert(generation != 0);

    const auto different_generation = std::bind(std::not_equal_to<Generation>(), std::placeholders::_1, generation);
    const Allocated::iterator first_page = state.allocated_pages.begin() + page;
    const Allocated::iterator last_page = std::find_if(first_page, state.allocated_pages.end(), different_generation);
    std::fill(first_page, last_page, 0);

    // TODO Decommit/protect freed memory.
}

uint32_t mem_available(MemState &state) {
    const size_t page_count = 1;
    const Allocated::iterator block = std::search_n(state.allocated_pages.begin(), state.allocated_pages.end(), page_count, 0);
    if (block == state.allocated_pages.end()) {
        assert(false);
        return 0;
    }

    const size_t block_page_index = block - state.allocated_pages.begin();
    const Address address = static_cast<Address>(GB(4) - static_cast<Address>(block_page_index * state.page_size));

    return address;
}

const char *mem_name(Address address, MemState &state) {
    const size_t page = address / state.page_size;
    assert(page >= 0);
    assert(page < state.allocated_pages.size());

    const Generation generation = state.allocated_pages[page];
    if (generation == 0) {
        return "UNALLOCATED";
    }

    const std::lock_guard<std::mutex> lock(state.generation_mutex);
    const GenerationNames::const_iterator found = state.generation_names.find(generation);
    assert(found != state.generation_names.end());
    if (found == state.generation_names.end()) {
        return "UNNAMED";
    }

    return found->second.c_str();
}

constexpr unsigned char thumb_breakpoint[2] = { 0x00, 0xBE };
constexpr unsigned char arm_breakpoint[4] = { 0x70, 0x00, 0x20, 0xE1 };

void add_breakpoint(MemState &mem, bool gdb, bool thumb_mode, uint32_t addr, BreakpointCallback callback) {
    Breakpoint last = {
        gdb,
        thumb_mode,
        { 0 },
        callback
    };
    if (thumb_mode) {
        std::memcpy(&last.data, &mem.memory[addr], sizeof(thumb_breakpoint));
        std::memcpy(&mem.memory[addr], thumb_breakpoint, sizeof(thumb_breakpoint));
    } else {
        std::memcpy(&last.data, &mem.memory[addr], sizeof(arm_breakpoint));
        std::memcpy(&mem.memory[addr], arm_breakpoint, sizeof(arm_breakpoint));
    }
    mem.breakpoints[addr] = last;
}

void remove_breakpoint(MemState &mem, uint32_t addr) {
    if (mem.breakpoints.find(addr) != mem.breakpoints.end()) {
        auto last = mem.breakpoints[addr];
        std::memcpy(&mem.memory[addr], &last.data, last.thumb_mode ? sizeof(thumb_breakpoint) : sizeof(arm_breakpoint));
        mem.breakpoints.erase(addr);
    }
}
