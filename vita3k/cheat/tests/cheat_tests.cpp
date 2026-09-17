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

#include <cheat/functions.h>

#include <mem/functions.h>
#include <mem/ptr.h>
#include <mem/state.h>

#include <gtest/gtest.h>

#include <cstring>
#include <fstream>

namespace {

constexpr uint32_t scratch_size = 0x1000;

class CheatTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(init(mem, false));

        scratch = alloc(mem, scratch_size, "cheat-tests");
        ASSERT_NE(scratch, 0u);
        memset(Ptr<uint8_t>(scratch).get(mem), 0, scratch_size);

        directory = fs::temp_directory_path() / "vita3k-cheat-tests";
        boost::system::error_code error;
        fs::remove_all(directory, error);
        fs::create_directories(directory);
    }

    void TearDown() override {
        boost::system::error_code error;
        fs::remove_all(directory, error);

        deinit_mem(mem);
    }

    fs::path write_named(const std::string &name, const std::string &contents) const {
        const auto path = directory / name;
        std::ofstream stream(path.c_str(), std::ios::binary | std::ios::trunc);
        stream << contents;
        return path;
    }

    // Writes a cheat file for `title_id` and hands back its path.
    fs::path write_file(const std::string &title_id, const std::string &contents) const {
        return write_named(title_id + ".psv", contents);
    }

    void run(const std::string &contents, uint32_t buttons = 0) {
        write_file(title_id, contents);
        ASSERT_TRUE(cheat::load(state, directory, title_id));
        cheat::set_enabled(state, true, mem, {});
        cheat::set_all_cheats_enabled(state, true, mem, {});
        cheat::set_buttons(state, buttons);
        cheat::apply(state, mem, {});
    }

    uint32_t at(uint32_t offset) const {
        return scratch + offset;
    }

    uint32_t read32(uint32_t offset) const {
        return *Ptr<uint32_t>(at(offset)).get(mem);
    }

    uint16_t read16(uint32_t offset) const {
        return *Ptr<uint16_t>(at(offset)).get(mem);
    }

    uint8_t read8(uint32_t offset) const {
        return *Ptr<uint8_t>(at(offset)).get(mem);
    }

    void write32(uint32_t offset, uint32_t value) const {
        *Ptr<uint32_t>(at(offset)).get(mem) = value;
    }

    const std::string title_id = "PCSA00001";
    MemState mem;
    Address scratch = 0;
    fs::path directory;
    CheatState state;
};

} // namespace

TEST_F(CheatTest, parses_declarations_and_codes) {
    const auto path = write_file(title_id,
        "# PCSA00001 Test Game\n"
        "\n"
        "_V0 Off by default\n"
        "$0200 81000000 0000000A\n"
        "$0200 81000004 0000000B\n"
        "\n"
        "_V1 On at boot\n"
        "$0100 81000008 00000063\n");

    const auto file = cheat::parse_cheat_file(path, title_id);

    ASSERT_EQ(file.cheats.size(), 2u);
    EXPECT_EQ(file.header, "PCSA00001 Test Game");
    EXPECT_EQ(file.cheats[0].name, "Off by default");
    EXPECT_FALSE(file.cheats[0].enabled_on_boot);
    EXPECT_EQ(file.cheats[0].lines.size(), 2u);
    EXPECT_EQ(file.cheats[1].name, "On at boot");
    EXPECT_TRUE(file.cheats[1].enabled_on_boot);
    EXPECT_EQ(file.cheats[1].lines[0].control, 0x0100);
    EXPECT_EQ(file.cheats[1].lines[0].first, 0x81000008u);
    EXPECT_EQ(file.cheats[1].lines[0].second, 0x63u);
}

TEST_F(CheatTest, drops_malformed_lines_and_empty_cheats) {
    const auto path = write_file(title_id,
        "_V0 Broken code\n"
        "$02OO 81000000 0000000A\n"
        "$0200 81000000\n"
        "$0200 81000004 0000000B\n"
        "\n"
        "_V0 No code at all\n");

    const auto file = cheat::parse_cheat_file(path, title_id);

    ASSERT_EQ(file.cheats.size(), 1u);
    EXPECT_EQ(file.cheats[0].name, "Broken code");
    ASSERT_EQ(file.cheats[0].lines.size(), 1u);
    EXPECT_EQ(file.cheats[0].lines[0].first, 0x81000004u);
}

TEST_F(CheatTest, only_boot_cheats_are_on_after_load) {
    write_file(title_id,
        "_V0 Off\n"
        "$0200 81000000 0000000A\n"
        "_V1 On\n"
        "$0200 81000004 0000000B\n");

    ASSERT_TRUE(cheat::load(state, directory, title_id));

    EXPECT_EQ(cheat::enabled_cheat_count(state), 1u);
    const auto file = cheat::snapshot(state);
    EXPECT_FALSE(file.cheats[0].enabled);
    EXPECT_TRUE(file.cheats[1].enabled);
}

TEST_F(CheatTest, write_code_honours_the_value_width) {
    write32(0, 0xFFFFFFFF);
    write32(4, 0xFFFFFFFF);
    write32(8, 0xFFFFFFFF);

    run(fmt::format("_V1 Write\n"
                    "$0000 {:08X} 000000AB\n"
                    "$0100 {:08X} 0000CDEF\n"
                    "$0200 {:08X} 12345678\n",
        at(0), at(4), at(8)));

    EXPECT_EQ(read8(0), 0xAB);
    EXPECT_EQ(read32(0), 0xFFFFFFAB);
    EXPECT_EQ(read16(4), 0xCDEF);
    EXPECT_EQ(read32(8), 0x12345678u);
}

TEST_F(CheatTest, mov_code_copies_the_source_to_the_destination) {
    write32(4, 0x0BADF00D);

    run(fmt::format("_V1 Mov\n"
                    "$5200 {:08X} {:08X}\n",
        at(0), at(4)));

    EXPECT_EQ(read32(0), 0x0BADF00Du);
}

TEST_F(CheatTest, compression_code_steps_the_address_and_the_value) {
    run(fmt::format("_V1 Compression\n"
                    "$4201 {:08X} 00000010\n"
                    "$0003 00000004 00000001\n",
        at(0)));

    EXPECT_EQ(read32(0), 0x10u);
    EXPECT_EQ(read32(4), 0x11u);
    EXPECT_EQ(read32(8), 0x12u);
    EXPECT_EQ(read32(12), 0u);
}

TEST_F(CheatTest, reads_only_the_section_of_the_requested_title) {
    const auto path = write_named("cheat.db",
        "########################################\n"
        ">>>Start Of VitaCheat Database (v0.1)<<<\n"
        "########################################\n"
        "_S PCSA00001\n"
        "_V0 Mine\n"
        "$0200 81000000 0000000A\n"
        "\n"
        "_S PCSA00002 Another Game\n"
        "_V1 Not mine\n"
        "$0200 81000004 0000000B\n");

    const auto mine = cheat::parse_cheat_file(path, title_id);
    ASSERT_EQ(mine.cheats.size(), 1u);
    EXPECT_EQ(mine.cheats[0].name, "Mine");
    EXPECT_FALSE(mine.cheats[0].enabled_on_boot);
    // The banner of the database is not the header of this game.
    EXPECT_TRUE(mine.header.empty());

    // The title id of a `_S` line can be followed by a game name.
    const auto other = cheat::parse_cheat_file(path, "PCSA00002");
    ASSERT_EQ(other.cheats.size(), 1u);
    EXPECT_EQ(other.cheats[0].name, "Not mine");
    EXPECT_TRUE(other.cheats[0].enabled_on_boot);

    EXPECT_TRUE(cheat::parse_cheat_file(path, "PCSA09999").cheats.empty());
}

TEST_F(CheatTest, finds_a_combined_database_by_its_section) {
    write_named("cheat.db",
        "_S PCSA00002\n"
        "_V0 Not mine\n"
        "$0200 81000000 0000000A\n"
        "_S PCSA00001\n"
        "_V0 Mine\n"
        "$0200 81000004 0000000B\n");

    EXPECT_EQ(fs_utils::path_to_utf8(cheat::find_cheat_file(directory, title_id).filename()), "cheat.db");
    EXPECT_TRUE(cheat::find_cheat_file(directory, "PCSA09999").empty());
}

TEST_F(CheatTest, a_file_named_after_the_title_wins_over_a_combined_database) {
    write_named("cheat.db", "_S PCSA00001\n_V0 From the database\n$0200 81000000 0000000A\n");
    write_file(title_id, "_V0 From its own file\n$0200 81000000 0000000A\n");

    const auto path = cheat::find_cheat_file(directory, title_id);
    EXPECT_EQ(fs_utils::path_to_utf8(path.filename()), title_id + ".psv");
}

TEST_F(CheatTest, saving_a_combined_database_leaves_the_other_sections_alone) {
    const auto path = write_named("cheat.db",
        "_S PCSA00001\n"
        "_V0 Mine\n"
        "$0200 81000000 0000000A\n"
        "_S PCSA00002\n"
        "_V1 Not mine\n"
        "$0200 81000004 0000000B\n");

    ASSERT_TRUE(cheat::load(state, directory, title_id));
    cheat::set_cheat_enabled(state, 0, true, mem, {});
    ASSERT_TRUE(cheat::save(state));

    const auto mine = cheat::parse_cheat_file(path, title_id);
    ASSERT_EQ(mine.cheats.size(), 1u);
    EXPECT_TRUE(mine.cheats[0].enabled_on_boot);

    const auto other = cheat::parse_cheat_file(path, "PCSA00002");
    ASSERT_EQ(other.cheats.size(), 1u);
    EXPECT_TRUE(other.cheats[0].enabled_on_boot);
}

TEST_F(CheatTest, pointer_compression_repeats_a_write_through_a_pointer) {
    // Shape the databases use: a pointer block, the `$77` line carrying the value, then the gaps.
    write32(0, at(0x40));

    run(fmt::format("_V1 Pointer compression\n"
                    "$7001 {:08X} 00000020\n"
                    "$7701 00000000 00000003\n"
                    "$0004 00000001 00000000\n",
        at(0)));

    // The 8-bit value 3 written four times, one byte apart.
    EXPECT_EQ(read32(0x60), 0x03030303u);
}

TEST_F(CheatTest, pointer_compression_follows_two_levels_and_steps_the_value) {
    write32(0, at(0x40));
    write32(0x78, at(0x100));

    run(fmt::format("_V1 Two levels\n"
                    "$7202 {:08X} 00000038\n"
                    "$7000 00000000 00000010\n"
                    "$7702 00000000 0000000A\n"
                    "$0003 00000004 00000001\n",
        at(0)));

    EXPECT_EQ(read32(0x110), 0x0Au);
    EXPECT_EQ(read32(0x114), 0x0Bu);
    EXPECT_EQ(read32(0x118), 0x0Cu);
}

TEST_F(CheatTest, pointer_write_follows_three_levels) {
    write32(0, at(0x40));
    write32(0x50, at(0x80));
    write32(0xA0, at(0x100));

    run(fmt::format("_V1 Three levels\n"
                    "$3203 {:08X} 00000010\n"
                    "$3200 00000000 00000020\n"
                    "$3200 00000000 00000030\n"
                    "$3300 00000000 01010101\n",
        at(0)));

    EXPECT_EQ(read32(0x130), 0x01010101u);
}

TEST_F(CheatTest, pointer_write_follows_one_level) {
    // The pointer at offset 0 points at offset 0x40, the code writes 0x20 past it.
    write32(0, at(0x40));

    run(fmt::format("_V1 Pointer write\n"
                    "$3201 {:08X} 00000020\n"
                    "$3300 00000000 CAFEBABE\n",
        at(0)));

    EXPECT_EQ(read32(0x60), 0xCAFEBABEu);
}

TEST_F(CheatTest, pointer_write_follows_two_levels_of_plain_continuation_lines) {
    // Layout the published databases use: follow-up lines leave their control word at `$0000`.
    write32(0, at(0x40));
    write32(0x40 + 0x5C, at(0x200));

    run(fmt::format("_V1 Two levels\n"
                    "$3202 {:08X} 0000005C\n"
                    "$0000 00000000 00000100\n"
                    "$0000 00000000 60BABE00\n",
        at(0)));

    EXPECT_EQ(read32(0x200 + 0x100), 0x60BABE00u);
}

TEST_F(CheatTest, condition_skips_every_line_of_the_pointer_write_it_guards) {
    write32(0x400, 0xFFFFFFFF);

    // A failed condition must skip the whole pointer write, but not the code below it.
    run(fmt::format("_V1 Guarded pointer write\n"
                    "$D201 {:08X} 12345678\n"
                    "$3202 {:08X} 0000005C\n"
                    "$0000 {:08X} 000000AB\n"
                    "$0000 00000000 3F6F0000\n"
                    "$0200 {:08X} 0000002A\n",
        at(0), at(0), at(0x400), at(0x500)));

    EXPECT_EQ(read32(0x400), 0xFFFFFFFFu);
    EXPECT_EQ(read32(0x500), 0x2Au);
}

TEST_F(CheatTest, pointer_mov_copies_through_both_chains) {
    write32(0, at(0x40)); // destination pointer
    write32(4, at(0x80)); // source pointer
    write32(0x84, 0x11223344);

    run(fmt::format("_V1 Pointer mov\n"
                    "$8201 {:08X} 00000004\n"
                    "$8800 00000000 00000000\n"
                    "$8601 {:08X} 00000004\n"
                    "$8900 00000000 00000000\n",
        at(0), at(4)));

    EXPECT_EQ(read32(0x44), 0x11223344u);
}

TEST_F(CheatTest, condition_guards_the_related_lines) {
    write32(0, 5);

    run(fmt::format("_V1 Condition\n"
                    "$D201 {:08X} 00000005\n"
                    "$0200 {:08X} 00000001\n"
                    "$D201 {:08X} 00000006\n"
                    "$0200 {:08X} 00000002\n"
                    "$0200 {:08X} 00000003\n",
        at(0), at(4), at(0), at(8), at(12)));

    EXPECT_EQ(read32(4), 1u); // guarded by a condition that holds
    EXPECT_EQ(read32(8), 0u); // guarded by a condition that does not
    EXPECT_EQ(read32(12), 3u); // past the guarded line, runs either way
}

TEST_F(CheatTest, button_pad_code_needs_every_button_of_the_mask) {
    const auto codes = fmt::format("_V1 Button\n"
                                   "$C201 00000001 00000300\n"
                                   "$0200 {:08X} 00000007\n",
        at(0));

    run(codes, 0x100);
    EXPECT_EQ(read32(0), 0u);

    cheat::set_buttons(state, 0x300);
    cheat::apply(state, mem, {});
    EXPECT_EQ(read32(0), 7u);
}

TEST_F(CheatTest, arm_write_is_undone_when_the_cheat_is_turned_off) {
    write32(0, 0xE1A00000);

    size_t invalidated = 0;
    const cheat::JitInvalidate invalidate = [&invalidated](uint32_t, size_t) { ++invalidated; };

    write_file(title_id,
        fmt::format("_V1 Nop it out\n"
                    "$A200 {:08X} EA01D709\n",
            at(0)));
    ASSERT_TRUE(cheat::load(state, directory, title_id));
    cheat::set_enabled(state, true, mem, invalidate);

    cheat::apply(state, mem, invalidate);
    EXPECT_EQ(read32(0), 0xEA01D709u);
    EXPECT_EQ(invalidated, 1u);

    // Running it again must not patch anything, the instruction is already there.
    cheat::apply(state, mem, invalidate);
    EXPECT_EQ(invalidated, 1u);

    cheat::set_cheat_enabled(state, 0, false, mem, invalidate);
    EXPECT_EQ(read32(0), 0xE1A00000u);
    EXPECT_EQ(invalidated, 2u);
}

TEST_F(CheatTest, master_switch_stops_and_restores_every_cheat) {
    run(fmt::format("_V1 Freeze\n"
                    "$0200 {:08X} 00000063\n",
        at(0)));

    EXPECT_EQ(read32(0), 0x63u);

    cheat::set_enabled(state, false, mem, {});
    write32(0, 0);
    cheat::apply(state, mem, {});
    EXPECT_EQ(read32(0), 0u);
}

TEST_F(CheatTest, saving_rewrites_the_boot_markers_only) {
    const auto path = write_file(title_id,
        "# PCSA00001 Test Game\r\n"
        "_V0 First\r\n"
        "$0200 81000000 0000000A\r\n"
        "_V1 Second\r\n"
        "$0200 81000004 0000000B\r\n");

    ASSERT_TRUE(cheat::load(state, directory, title_id));
    cheat::set_cheat_enabled(state, 0, true, mem, {});
    cheat::set_cheat_enabled(state, 1, false, mem, {});
    ASSERT_TRUE(cheat::save(state));

    const auto saved = cheat::parse_cheat_file(path, title_id);
    ASSERT_EQ(saved.cheats.size(), 2u);
    EXPECT_TRUE(saved.cheats[0].enabled_on_boot);
    EXPECT_FALSE(saved.cheats[1].enabled_on_boot);
    EXPECT_EQ(saved.header, "PCSA00001 Test Game");
    EXPECT_EQ(saved.cheats[0].lines[0].first, 0x81000000u);
}
