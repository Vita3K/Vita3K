// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mem/atomic.h>
#include <mem/state.h>
#include <vector>

namespace {
constexpr uint32_t page_size = 4096;
constexpr uint64_t address_limit = uint64_t{ 1 } << 32;
uint8_t *native_pointer(const MemState &mem, Address addr) {
    return (mem.use_page_table ? mem.page_table[addr / page_size] : mem.memory.get()) + addr;
}
uint8_t *backing_pointer(const MemState &mem, const MemState::BackingPage &page) {
    return page.retained ? page.retained.get() : native_pointer(mem, page.address);
}
bool valid_range(const MemState &mem, Address addr, uint32_t size) {
    if (!addr || !size || (addr & 4095) || (size & 4095) || uint64_t(addr) + size > address_limit)
        return false;
    for (uint64_t p = addr; p < uint64_t(addr) + size; p += page_size)
        if (!is_valid_addr(mem, static_cast<Address>(p)))
            return false;
    return true;
}
PagePtr native_entry(const MemState &mem, Address page) {
    return native_pointer(mem, page) - page;
}
// Locked. The JIT reads entries concurrently: stores are single aligned pointer writes.
PagePtr page_entry(const MemState &mem, Address page) {
    const auto it = mem.guest_mappings.find(page);
    if (it == mem.guest_mappings.end())
        return is_valid_addr(mem, page) ? native_entry(mem, page) : nullptr;
    const auto &mapping = it->second;
    if (!mapping.backing || mapping.perm != MemPerm::ReadWrite)
        return nullptr;
    return backing_pointer(mem, *mapping.backing) - page;
}
void set_entry(MemState &mem, Address page, PagePtr entry) {
    std::atomic_ref<PagePtr>(mem.guest_page_table[page / page_size]).store(entry, std::memory_order_release);
}
void refresh_entries(MemState &mem, Address addr, uint32_t size) {
    if (!mem.guest_page_table)
        return;
    for (uint64_t p = addr & ~4095u; p < uint64_t(addr) + size; p += page_size)
        set_entry(mem, static_cast<Address>(p), page_entry(mem, static_cast<Address>(p)));
}
void ensure_page_table(MemState &mem) {
    if (mem.guest_page_table)
        return;
    mem.guest_page_table = std::make_unique<PagePtr[]>(address_limit / page_size);
    refresh_entries(mem, 0, static_cast<uint32_t>(address_limit - page_size));
    set_entry(mem, static_cast<Address>(address_limit - page_size), page_entry(mem, static_cast<Address>(address_limit - page_size)));
}
MemState::VirtualPage &get_page(MemState &mem, Address addr) {
    auto [it, added] = mem.guest_mappings.try_emplace(addr);
    if (added) {
        it->second.backing = std::make_shared<MemState::BackingPage>(addr);
        it->second.perm = MemPerm::ReadWrite;
    }
    return it->second;
}
// Preserve physical data before its native allocation can be cleared/reused.
void retain_backing(MemState &mem, Address addr, uint32_t size) {
    std::vector<Address> moved;
    for (auto &[key, page] : mem.guest_mappings) {
        auto &backing = page.backing;
        if (backing && !backing->retained && backing->address >= addr && uint64_t(backing->address) < uint64_t(addr) + size) {
            auto copy = std::make_unique<uint8_t[]>(page_size);
            std::memcpy(copy.get(), native_pointer(mem, backing->address), page_size);
            backing->retained = std::move(copy);
        }
        if (backing && backing->retained && backing->address >= addr && uint64_t(backing->address) < uint64_t(addr) + size)
            moved.push_back(key);
    }
    // Aliases of a retained page now read the copy.
    for (const auto key : moved)
        if (mem.guest_page_table)
            set_entry(mem, key, page_entry(mem, key));
}
bool check_access_locked(MemState &mem, Address addr, uint32_t size, bool write, Address &fault) {
    if (!size || uint64_t(addr) + size > address_limit) {
        fault = addr;
        return false;
    }
    for (uint64_t p = addr; p < uint64_t(addr) + size;) {
        const Address page = static_cast<Address>(p) & ~4095u;
        const auto it = mem.guest_mappings.find(page);
        if (!is_valid_addr(mem, static_cast<Address>(p)) || (it != mem.guest_mappings.end() && (!it->second.backing || it->second.perm == MemPerm::None || (write && it->second.perm == MemPerm::ReadOnly)))) {
            fault = static_cast<Address>(p);
            return false;
        }
        p = uint64_t(page) + page_size;
    }
    return true;
}
} // namespace

bool map_guest_memory(MemState &mem, Address dest, uint32_t size, Address source, MemPerm perm) {
    if (!valid_range(mem, dest, size) || !valid_range(mem, source, size))
        return false;
    std::lock_guard lock(mem.guest_mapping_mutex);
    // Snapshot first: overlapping mappings and aliases of aliases cannot cycle.
    std::vector<std::shared_ptr<MemState::BackingPage>> pages;
    pages.reserve(size / page_size);
    for (uint32_t offset = 0; offset < size; offset += page_size) {
        auto &page = get_page(mem, source + offset);
        if (!page.backing)
            return false;
        pages.push_back(page.backing);
    }
    for (uint32_t offset = 0; offset < size; offset += page_size) {
        auto &backing = pages[offset / page_size];
        if (dest + offset != backing->address && std::find(backing->aliases.begin(), backing->aliases.end(), dest + offset) == backing->aliases.end())
            backing->aliases.push_back(dest + offset);
        mem.guest_mappings[dest + offset] = { backing, perm };
    }
    ensure_page_table(mem);
    refresh_entries(mem, source, size);
    refresh_entries(mem, dest, size);
    mem.guest_mappings_active.store(true, std::memory_order_release);
    return true;
}
bool commit_guest_memory(MemState &mem, Address dest, uint32_t size, MemPerm perm) {
    if (!valid_range(mem, dest, size))
        return false;
    std::lock_guard lock(mem.guest_mapping_mutex);
    for (uint32_t offset = 0; offset < size; offset += page_size) {
        auto &page = get_page(mem, dest + offset);
        if (!page.backing) {
            page.backing = std::make_shared<MemState::BackingPage>(dest + offset);
            std::memset(native_pointer(mem, dest + offset), 0, page_size);
        }
        page.perm = perm;
    }
    ensure_page_table(mem);
    refresh_entries(mem, dest, size);
    mem.guest_mappings_active.store(true, std::memory_order_release);
    return true;
}
bool protect_guest_memory(MemState &mem, Address dest, uint32_t size, MemPerm perm) {
    if (!valid_range(mem, dest, size))
        return false;
    std::lock_guard lock(mem.guest_mapping_mutex);
    for (uint32_t offset = 0; offset < size; offset += page_size)
        get_page(mem, dest + offset).perm = perm;
    ensure_page_table(mem);
    refresh_entries(mem, dest, size);
    mem.guest_mappings_active.store(true, std::memory_order_release);
    return true;
}
bool decommit_guest_memory(MemState &mem, Address dest, uint32_t size) {
    if (!valid_range(mem, dest, size))
        return false;
    std::lock_guard lock(mem.guest_mapping_mutex);
    retain_backing(mem, dest, size);
    for (uint32_t offset = 0; offset < size; offset += page_size)
        mem.guest_mappings[dest + offset] = {};
    ensure_page_table(mem);
    refresh_entries(mem, dest, size);
    mem.guest_mappings_active.store(true, std::memory_order_release);
    return true;
}
void release_guest_mappings(MemState &mem, Address dest, uint32_t size) {
    if (!mem.guest_mappings_active.load(std::memory_order_acquire))
        return;
    std::lock_guard lock(mem.guest_mapping_mutex);
    retain_backing(mem, dest, size);
    auto first = mem.guest_mappings.lower_bound(dest);
    auto last = uint64_t(dest) + size == address_limit ? mem.guest_mappings.end() : mem.guest_mappings.lower_bound(dest + size);
    mem.guest_mappings.erase(first, last);
    // Called before the allocator frees the range: null it, a later allocation heals on first access.
    if (mem.guest_page_table)
        for (uint64_t p = dest; p < uint64_t(dest) + size; p += page_size)
            set_entry(mem, static_cast<Address>(p), nullptr);
}
void refresh_guest_page_table(MemState &mem, Address addr, uint32_t size) {
    if (!mem.guest_mappings_active.load(std::memory_order_acquire))
        return;
    std::lock_guard lock(mem.guest_mapping_mutex);
    refresh_entries(mem, addr, size);
}
uint8_t *resolve_guest_pointer(const MemState &mem, Address addr) {
    std::lock_guard lock(mem.guest_mapping_mutex);
    auto it = mem.guest_mappings.find(addr & ~4095u);
    if (it == mem.guest_mappings.end())
        return native_pointer(mem, addr);
    return it->second.backing ? backing_pointer(mem, *it->second.backing) + (addr & 4095u) : nullptr;
}
std::vector<std::pair<Address, uint32_t>> guest_alias_ranges(const MemState &mem, Address addr, uint32_t size) {
    std::vector<std::pair<Address, uint32_t>> ranges{ { addr, size } };
    if (!size)
        return ranges;
    std::lock_guard lock(mem.guest_mapping_mutex);
    const uint64_t end = uint64_t(addr) + size;
    const auto outside = [&](Address page) { return page + page_size <= addr || page >= end; };
    for (auto it = mem.guest_mappings.lower_bound(addr & ~4095u); it != mem.guest_mappings.end() && it->first < end; ++it) {
        const auto &backing = it->second.backing;
        if (!backing || (backing.use_count() == 1 && backing->address == it->first))
            continue;
        // The page's other views: its backing's home address and every address still mapped to it.
        if (outside(backing->address))
            ranges.emplace_back(backing->address, page_size);
        auto &aliases = backing->aliases;
        aliases.erase(std::remove_if(aliases.begin(), aliases.end(), [&](Address alias) {
            const auto found = mem.guest_mappings.find(alias);
            return found == mem.guest_mappings.end() || found->second.backing != backing;
        }),
            aliases.end());
        for (const auto alias : aliases)
            if (outside(alias))
                ranges.emplace_back(alias, page_size);
    }
    return ranges;
}
bool check_guest_memory_access(MemState &mem, Address addr, uint32_t size, bool write, Address &fault) {
    std::lock_guard lock(mem.guest_mapping_mutex);
    return check_access_locked(mem, addr, size, write, fault);
}
bool compare_exchange_guest_memory(MemState &mem, Address addr, uint32_t size, uint64_t value, uint64_t expected, bool &swapped, Address &fault) {
    std::lock_guard lock(mem.guest_mapping_mutex);
    swapped = false;
    // ARM exclusive accesses require natural alignment. Keep validation,
    // translation and CAS together so a remap cannot invalidate the pointer.
    if ((size != 1 && size != 2 && size != 4 && size != 8) || (addr & (size - 1))) {
        fault = addr;
        return false;
    }
    if (!check_access_locked(mem, addr, size, true, fault))
        return false;
    auto it = mem.guest_mappings.find(addr & ~4095u);
    auto ptr = it == mem.guest_mappings.end() ? native_pointer(mem, addr) : backing_pointer(mem, *it->second.backing) + (addr & 4095u);
    switch (size) {
    case 1: swapped = atomic_compare_and_swap(reinterpret_cast<volatile uint8_t *>(ptr), uint8_t(value), uint8_t(expected)); break;
    case 2: swapped = atomic_compare_and_swap(reinterpret_cast<volatile uint16_t *>(ptr), uint16_t(value), uint16_t(expected)); break;
    case 4: swapped = atomic_compare_and_swap(reinterpret_cast<volatile uint32_t *>(ptr), uint32_t(value), uint32_t(expected)); break;
    case 8: swapped = atomic_compare_and_swap(reinterpret_cast<volatile uint64_t *>(ptr), value, expected); break;
    }
    return true;
}
bool access_guest_memory(MemState &mem, Address addr, void *data, uint32_t size, bool write, Address &fault) {
    std::lock_guard lock(mem.guest_mapping_mutex);
    // A cross-page fault must not leave a partial store.
    if (!check_access_locked(mem, addr, size, write, fault))
        return false;
    // Memory allocated after the table was built (or reused after a free) starts null: heal it.
    if (mem.guest_page_table && !mem.guest_page_table[addr / page_size])
        set_entry(mem, addr & ~4095u, page_entry(mem, addr & ~4095u));
    auto bytes = static_cast<uint8_t *>(data);
    while (size) {
        auto it = mem.guest_mappings.find(addr & ~4095u);
        auto ptr = it == mem.guest_mappings.end() ? native_pointer(mem, addr) : backing_pointer(mem, *it->second.backing) + (addr & 4095u);
        const auto count = std::min(size, page_size - (addr & 4095u));
        if (write)
            std::memcpy(ptr, bytes, count);
        else
            std::memcpy(bytes, ptr, count);
        addr += count;
        bytes += count;
        size -= count;
    }
    return true;
}
