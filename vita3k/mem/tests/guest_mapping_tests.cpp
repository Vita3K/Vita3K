// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mem/functions.h>
#include <mem/ptr.h>
#include <mem/state.h>
#include <util/align.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace {
constexpr uint32_t page_size = KiB(4);
constexpr Address source_address = 0x80000000U;
constexpr Address first_alias_address = 0x80100000U;
constexpr Address second_alias_address = 0x80200000U;

struct TestMemory {
    MemState mem;
    bool initialized;

    explicit TestMemory(bool use_page_table)
        : initialized(init(mem, use_page_table)) {
    }

    ~TestMemory() {
        if (initialized)
            deinit_mem(mem);
    }
};

template <typename Function>
void for_each_memory_mode(Function &&function) {
    for (const bool use_page_table : { false, true }) {
        TestMemory test_memory(use_page_table);
        ASSERT_TRUE(test_memory.initialized);
        if (!test_memory.initialized)
            continue;
        SCOPED_TRACE(use_page_table ? "page table mode" : "native mode");
        function(test_memory.mem);
    }
}

Address allocate_page(MemState &mem, Address address, const char *name) {
    return alloc_at(mem, address, page_size, name);
}
} // namespace

TEST(guest_mapping, bidirectional_and_multiple_aliases) {
    for_each_memory_mode([](MemState &mem) {
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_source"), 0U);
        ASSERT_NE(allocate_page(mem, first_alias_address, "guest_mapping_alias_1"), 0U);
        ASSERT_NE(allocate_page(mem, second_alias_address, "guest_mapping_alias_2"), 0U);

        uint32_t value = 0x11223344;
        Address fault = 0;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &value, sizeof(value), true, fault));
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, source_address, MemPerm::ReadWrite));
        ASSERT_TRUE(map_guest_memory(mem, second_alias_address, page_size, source_address, MemPerm::ReadWrite));

        uint32_t read = 0;
        ASSERT_TRUE(access_guest_memory(mem, first_alias_address, &read, sizeof(read), false, fault));
        EXPECT_EQ(read, value);
        ASSERT_TRUE(access_guest_memory(mem, second_alias_address, &read, sizeof(read), false, fault));
        EXPECT_EQ(read, value);

        value = 0xAABBCCDD;
        ASSERT_TRUE(access_guest_memory(mem, first_alias_address + 4, &value, sizeof(value), true, fault));
        ASSERT_TRUE(access_guest_memory(mem, source_address + 4, &read, sizeof(read), false, fault));
        EXPECT_EQ(read, value);
        ASSERT_TRUE(access_guest_memory(mem, second_alias_address + 4, &read, sizeof(read), false, fault));
        EXPECT_EQ(read, value);

        EXPECT_EQ(resolve_guest_pointer(mem, first_alias_address + 4), resolve_guest_pointer(mem, source_address + 4));
    });
}

TEST(guest_mapping, remap_and_independent_permissions) {
    for_each_memory_mode([](MemState &mem) {
        constexpr Address second_source_address = 0x80300000U;
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_source_a"), 0U);
        ASSERT_NE(allocate_page(mem, second_source_address, "guest_mapping_source_b"), 0U);
        ASSERT_NE(allocate_page(mem, first_alias_address, "guest_mapping_remap"), 0U);

        Address fault = 0;
        uint32_t source_a = 0x11111111;
        uint32_t source_b = 0x22222222;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &source_a, sizeof(source_a), true, fault));
        ASSERT_TRUE(access_guest_memory(mem, second_source_address, &source_b, sizeof(source_b), true, fault));

        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, source_address, MemPerm::ReadWrite));
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, second_source_address, MemPerm::ReadWrite));

        uint32_t value = 0;
        ASSERT_TRUE(access_guest_memory(mem, first_alias_address, &value, sizeof(value), false, fault));
        EXPECT_EQ(value, source_b);

        ASSERT_TRUE(protect_guest_memory(mem, first_alias_address, page_size, MemPerm::ReadOnly));
        value = 0x33333333;
        EXPECT_FALSE(access_guest_memory(mem, first_alias_address, &value, sizeof(value), true, fault));
        EXPECT_EQ(fault, first_alias_address);

        // The alias protection does not change the permissions of its source.
        source_b = 0x44444444;
        ASSERT_TRUE(access_guest_memory(mem, second_source_address, &source_b, sizeof(source_b), true, fault));
        ASSERT_TRUE(access_guest_memory(mem, first_alias_address, &value, sizeof(value), false, fault));
        EXPECT_EQ(value, source_b);

        ASSERT_TRUE(protect_guest_memory(mem, first_alias_address, page_size, MemPerm::ReadWrite));
        value = 0x55555555;
        ASSERT_TRUE(access_guest_memory(mem, first_alias_address, &value, sizeof(value), true, fault));
        ASSERT_TRUE(access_guest_memory(mem, second_source_address, &source_b, sizeof(source_b), false, fault));
        EXPECT_EQ(source_b, value);
    });
}

TEST(guest_mapping, cross_page_write_is_atomic) {
    for_each_memory_mode([](MemState &mem) {
        ASSERT_NE(alloc(mem, page_size * 2, "guest_mapping_cross_page_source", source_address), 0U);
        ASSERT_NE(alloc(mem, page_size * 2, "guest_mapping_cross_page_alias", first_alias_address), 0U);

        Address fault = 0;
        std::array<uint8_t, 4> initial{ 0x11, 0x11, 0x22, 0x22 };
        ASSERT_TRUE(access_guest_memory(mem, source_address + page_size - 2, initial.data(), initial.size(), true, fault));
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size * 2, source_address, MemPerm::ReadWrite));
        ASSERT_TRUE(protect_guest_memory(mem, first_alias_address + page_size, page_size, MemPerm::None));

        std::array<uint8_t, 4> update{ 0xAA, 0xBB, 0xCC, 0xDD };
        EXPECT_FALSE(access_guest_memory(mem, first_alias_address + page_size - 2, update.data(), update.size(), true, fault));
        EXPECT_EQ(fault, first_alias_address + page_size);

        std::array<uint8_t, 4> actual{};
        ASSERT_TRUE(access_guest_memory(mem, source_address + page_size - 2, actual.data(), actual.size(), false, fault));
        EXPECT_EQ(actual, initial);
    });
}

TEST(guest_mapping, cross_page_access_across_distinct_backings) {
    for_each_memory_mode([](MemState &mem) {
        constexpr Address second_source_address = 0x80300000U;
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_cross_source_a"), 0U);
        ASSERT_NE(allocate_page(mem, second_source_address, "guest_mapping_cross_source_b"), 0U);
        ASSERT_NE(alloc(mem, page_size * 2, "guest_mapping_cross_alias", first_alias_address), 0U);

        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, source_address, MemPerm::ReadWrite));
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address + page_size, page_size, second_source_address, MemPerm::ReadWrite));

        Address fault = 0;
        std::array<uint8_t, 4> initial{ 0x11, 0x22, 0x33, 0x44 };
        ASSERT_TRUE(access_guest_memory(mem, source_address + page_size - 2, initial.data(), 2, true, fault));
        ASSERT_TRUE(access_guest_memory(mem, second_source_address, initial.data() + 2, 2, true, fault));

        std::array<uint8_t, 4> actual{};
        ASSERT_TRUE(access_guest_memory(mem, first_alias_address + page_size - 2, actual.data(), actual.size(), false, fault));
        EXPECT_EQ(actual, initial);

        std::array<uint8_t, 4> update{ 0xAA, 0xBB, 0xCC, 0xDD };
        ASSERT_TRUE(access_guest_memory(mem, first_alias_address + page_size - 2, update.data(), update.size(), true, fault));
        ASSERT_TRUE(access_guest_memory(mem, source_address + page_size - 2, actual.data(), 2, false, fault));
        EXPECT_EQ(actual[0], update[0]);
        EXPECT_EQ(actual[1], update[1]);
        ASSERT_TRUE(access_guest_memory(mem, second_source_address, actual.data() + 2, 2, false, fault));
        EXPECT_EQ(actual[2], update[2]);
        EXPECT_EQ(actual[3], update[3]);
    });
}

TEST(guest_mapping, commit_recreates_decommitted_backing) {
    for_each_memory_mode([](MemState &mem) {
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_commit"), 0U);
        Address fault = 0;
        uint32_t value = 0x01020304;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &value, sizeof(value), true, fault));
        ASSERT_TRUE(decommit_guest_memory(mem, source_address, page_size));
        EXPECT_FALSE(access_guest_memory(mem, source_address, &value, sizeof(value), false, fault));
        ASSERT_TRUE(commit_guest_memory(mem, source_address, page_size, MemPerm::ReadWrite));

        uint32_t committed_value = 0;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &committed_value, sizeof(committed_value), false, fault));
        EXPECT_EQ(committed_value, 0U);
        value = 0xAABBCCDD;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &value, sizeof(value), true, fault));
        ASSERT_TRUE(access_guest_memory(mem, source_address, &committed_value, sizeof(committed_value), false, fault));
        EXPECT_EQ(committed_value, value);
    });
}

TEST(guest_mapping, decommit_source_free_and_reuse_retains_alias_data) {
    for_each_memory_mode([](MemState &mem) {
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_retained_source"), 0U);
        ASSERT_NE(allocate_page(mem, first_alias_address, "guest_mapping_retained_alias"), 0U);

        Address fault = 0;
        uint32_t value = 0xCAFEBABE;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &value, sizeof(value), true, fault));
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, source_address, MemPerm::ReadWrite));

        ASSERT_TRUE(decommit_guest_memory(mem, first_alias_address, page_size));
        EXPECT_EQ(resolve_guest_pointer(mem, first_alias_address), nullptr);
        EXPECT_FALSE(access_guest_memory(mem, first_alias_address, &value, sizeof(value), false, fault));
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, source_address, MemPerm::ReadWrite));

        ASSERT_TRUE(decommit_guest_memory(mem, source_address, page_size));
        EXPECT_FALSE(access_guest_memory(mem, source_address, &value, sizeof(value), false, fault));
        ASSERT_TRUE(access_guest_memory(mem, first_alias_address, &value, sizeof(value), false, fault));
        EXPECT_EQ(value, 0xCAFEBABEU);

        free(mem, source_address);
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_reused_source"), 0U);
        uint32_t replacement = 0x12345678;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &replacement, sizeof(replacement), true, fault));

        // Freeing and reusing the source does not invalidate the retained alias.
        ASSERT_TRUE(access_guest_memory(mem, first_alias_address, &value, sizeof(value), false, fault));
        EXPECT_EQ(value, 0xCAFEBABEU);
        EXPECT_NE(value, replacement);
    });
}

TEST(guest_mapping, freeing_source_without_guest_decommit_retains_alias_data) {
    for_each_memory_mode([](MemState &mem) {
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_free_source"), 0U);
        ASSERT_NE(allocate_page(mem, first_alias_address, "guest_mapping_free_alias"), 0U);

        Address fault = 0;
        uint32_t value = 0xDEADBEEF;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &value, sizeof(value), true, fault));
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, source_address, MemPerm::ReadWrite));

        free(mem, source_address);
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_free_reused"), 0U);
        uint32_t replacement = 0x12345678;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &replacement, sizeof(replacement), true, fault));

        ASSERT_TRUE(access_guest_memory(mem, first_alias_address, &value, sizeof(value), false, fault));
        EXPECT_EQ(value, 0xDEADBEEFU);
        EXPECT_NE(value, replacement);
    });
}

TEST(guest_mapping, rejects_invalid_ranges) {
    for_each_memory_mode([](MemState &mem) {
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_invalid_source"), 0U);
        ASSERT_NE(allocate_page(mem, first_alias_address, "guest_mapping_invalid_dest"), 0U);
        constexpr Address unallocated = first_alias_address + page_size;
        constexpr Address overflow = 0xFFFFF000U;

        EXPECT_FALSE(map_guest_memory(mem, first_alias_address + 1, page_size, source_address, MemPerm::ReadWrite));
        EXPECT_FALSE(map_guest_memory(mem, first_alias_address, page_size - 1, source_address, MemPerm::ReadWrite));
        EXPECT_FALSE(map_guest_memory(mem, first_alias_address, page_size, source_address + 1, MemPerm::ReadWrite));
        EXPECT_FALSE(map_guest_memory(mem, unallocated, page_size, source_address, MemPerm::ReadWrite));
        EXPECT_FALSE(map_guest_memory(mem, overflow, page_size * 2, source_address, MemPerm::ReadWrite));
        EXPECT_FALSE(map_guest_memory(mem, first_alias_address, page_size, 0, MemPerm::ReadWrite));

        EXPECT_FALSE(commit_guest_memory(mem, unallocated, page_size, MemPerm::ReadWrite));
        EXPECT_FALSE(protect_guest_memory(mem, unallocated, page_size, MemPerm::ReadOnly));
        EXPECT_FALSE(decommit_guest_memory(mem, unallocated, page_size));

        uint8_t value = 0;
        Address fault = 0;
        EXPECT_FALSE(access_guest_memory(mem, unallocated, &value, sizeof(value), false, fault));
        EXPECT_EQ(fault, unallocated);
        EXPECT_FALSE(access_guest_memory(mem, overflow, &value, sizeof(value), false, fault));
        EXPECT_EQ(fault, overflow);
    });
}

TEST(guest_mapping, exclusive_store_checks_alias_permissions) {
    for_each_memory_mode([](MemState &mem) {
        ASSERT_NE(allocate_page(mem, source_address, "exclusive_source"), 0U);
        ASSERT_NE(allocate_page(mem, first_alias_address, "exclusive_alias"), 0U);
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, source_address, MemPerm::ReadWrite));
        Address fault = 0;
        bool swapped = false;
        uint64_t value = 7;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &value, sizeof(value), true, fault));
        ASSERT_TRUE(compare_exchange_guest_memory(mem, first_alias_address, 8, 19, 7, swapped, fault));
        EXPECT_TRUE(swapped);
        ASSERT_TRUE(access_guest_memory(mem, source_address, &value, sizeof(value), false, fault));
        EXPECT_EQ(value, 19U);
        ASSERT_TRUE(compare_exchange_guest_memory(mem, first_alias_address, 8, 23, 7, swapped, fault));
        EXPECT_FALSE(swapped);
        ASSERT_TRUE(protect_guest_memory(mem, first_alias_address, page_size, MemPerm::ReadOnly));
        EXPECT_FALSE(compare_exchange_guest_memory(mem, first_alias_address, 8, 23, 19, swapped, fault));
        EXPECT_EQ(fault, first_alias_address);
        EXPECT_FALSE(compare_exchange_guest_memory(mem, source_address + 1, 8, 23, 19, swapped, fault));
        ASSERT_TRUE(access_guest_memory(mem, source_address, &value, sizeof(value), false, fault));
        EXPECT_EQ(value, 19U);
    });
}

TEST(guest_mapping, alias_ranges_follow_remaps) {
    for_each_memory_mode([](MemState &mem) {
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_source"), 0U);
        ASSERT_NE(allocate_page(mem, first_alias_address, "guest_mapping_alias_1"), 0U);
        ASSERT_NE(allocate_page(mem, second_alias_address, "guest_mapping_alias_2"), 0U);
        const auto has = [](const auto &ranges, Address addr) {
            return std::any_of(ranges.begin(), ranges.end(), [addr](const auto &r) { return r.first == addr; });
        };

        // A page that is only committed has no other view.
        ASSERT_TRUE(commit_guest_memory(mem, second_alias_address, page_size, MemPerm::ReadWrite));
        EXPECT_EQ(guest_alias_ranges(mem, second_alias_address + 16, 32).size(), 1U);

        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, source_address, MemPerm::ReadWrite));
        auto ranges = guest_alias_ranges(mem, source_address + 16, 32);
        EXPECT_TRUE(has(ranges, first_alias_address));
        ranges = guest_alias_ranges(mem, first_alias_address + 16, 32);
        EXPECT_TRUE(has(ranges, source_address));

        // Remapping the alias elsewhere drops it from the source's views.
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, second_alias_address, MemPerm::ReadWrite));
        ranges = guest_alias_ranges(mem, source_address + 16, 32);
        EXPECT_FALSE(has(ranges, first_alias_address));
        ranges = guest_alias_ranges(mem, second_alias_address + 16, 32);
        EXPECT_TRUE(has(ranges, first_alias_address));
    });
}

TEST(guest_mapping, page_table_tracks_aliases_and_permissions) {
    for_each_memory_mode([](MemState &mem) {
        ASSERT_NE(allocate_page(mem, source_address, "guest_mapping_source"), 0U);
        ASSERT_NE(allocate_page(mem, first_alias_address, "guest_mapping_alias_1"), 0U);
        ASSERT_NE(allocate_page(mem, second_alias_address, "guest_mapping_alias_2"), 0U);
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, source_address, MemPerm::ReadWrite));
        ASSERT_TRUE(mem.guest_page_table);
        const auto entry = [&mem](Address addr) { return mem.guest_page_table[addr / page_size]; };

        // A read/write alias resolves to the source's bytes; so does Ptr<T>.
        ASSERT_NE(entry(first_alias_address), nullptr);
        EXPECT_EQ(entry(first_alias_address) + first_alias_address + 8, resolve_guest_pointer(mem, source_address + 8));
        EXPECT_EQ(Ptr<uint32_t>(first_alias_address + 8).get(mem), reinterpret_cast<uint32_t *>(resolve_guest_pointer(mem, source_address + 8)));
        // Unmapped memory keeps its native base; unallocated memory takes the checked path.
        EXPECT_EQ(entry(second_alias_address) + second_alias_address, resolve_guest_pointer(mem, second_alias_address));
        EXPECT_EQ(entry(0x90000000U), nullptr);

        ASSERT_TRUE(protect_guest_memory(mem, first_alias_address, page_size, MemPerm::ReadOnly));
        EXPECT_EQ(entry(first_alias_address), nullptr);
        ASSERT_TRUE(protect_guest_memory(mem, first_alias_address, page_size, MemPerm::ReadWrite));
        EXPECT_NE(entry(first_alias_address), nullptr);

        // Decommitting the source keeps the alias on the retained copy; the source itself faults.
        uint32_t value = 0x55667788;
        Address fault = 0;
        ASSERT_TRUE(access_guest_memory(mem, source_address, &value, sizeof(value), true, fault));
        ASSERT_TRUE(decommit_guest_memory(mem, source_address, page_size));
        EXPECT_EQ(entry(source_address), nullptr);
        ASSERT_NE(entry(first_alias_address), nullptr);
        EXPECT_EQ(*reinterpret_cast<uint32_t *>(entry(first_alias_address) + first_alias_address), value);

        free(mem, second_alias_address);
        EXPECT_EQ(entry(second_alias_address), nullptr);
    });
}

TEST(guest_mapping, copies_split_across_non_contiguous_backing) {
    for_each_memory_mode([](MemState &mem) {
        // Two adjacent alias pages backed by two non-adjacent source pages.
        constexpr Address second_source = source_address + 0x10000;
        ASSERT_NE(alloc_at(mem, source_address, page_size, "src_a"), 0U);
        ASSERT_NE(alloc_at(mem, second_source, page_size, "src_b"), 0U);
        ASSERT_NE(alloc_at(mem, first_alias_address, 2 * page_size, "alias"), 0U);
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address, page_size, source_address, MemPerm::ReadWrite));
        ASSERT_TRUE(map_guest_memory(mem, first_alias_address + page_size, page_size, second_source, MemPerm::ReadWrite));
        EXPECT_EQ(contiguous_guest_pointer(mem, first_alias_address + page_size - 8, 16), nullptr);
        EXPECT_NE(contiguous_guest_pointer(mem, first_alias_address + 8, 16), nullptr);

        Address fault = 0;
        ASSERT_TRUE(fill_guest_memory(mem, first_alias_address + page_size - 8, 0xAB, 16, fault));
        uint8_t a = 0, b = 0;
        ASSERT_TRUE(access_guest_memory(mem, source_address + page_size - 1, &a, 1, false, fault));
        ASSERT_TRUE(access_guest_memory(mem, second_source + 7, &b, 1, false, fault));
        EXPECT_EQ(a, 0xAB);
        EXPECT_EQ(b, 0xAB);

        // A copy out of the split range lands in a plain page in order.
        ASSERT_NE(allocate_page(mem, second_alias_address, "dst"), 0U);
        ASSERT_TRUE(copy_guest_memory(mem, second_alias_address, first_alias_address + page_size - 8, 16, fault));
        std::array<uint8_t, 16> out{};
        ASSERT_TRUE(access_guest_memory(mem, second_alias_address, out.data(), 16, false, fault));
        for (const uint8_t v : out)
            EXPECT_EQ(v, 0xAB);

        // HLE copies ignore guest write protection (the app would handle the abort and retry).
        ASSERT_TRUE(protect_guest_memory(mem, first_alias_address, page_size, MemPerm::ReadOnly));
        ASSERT_TRUE(fill_guest_memory(mem, first_alias_address, 0x5A, 4, fault));
        ASSERT_TRUE(access_guest_memory(mem, source_address, &a, 1, false, fault));
        EXPECT_EQ(a, 0x5A);
    });
}
