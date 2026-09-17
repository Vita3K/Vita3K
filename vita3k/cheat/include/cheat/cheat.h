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

#include <util/fs.h>

#include <cstdint>
#include <string>
#include <vector>

// FinalCheat / VitaCheat `*.psv` cheat database format, described in cheat/README.md.
namespace cheat {

// Code type identifier, the first hexadecimal digit of the `$XXXX` control word.
enum class CodeType : uint8_t {
    write = 0x0, //!< `$0X00 <address> <value>`
    pointer_write = 0x3, //!< `$3X<level> <address> <offset>` ... `$33XX <offset> <value>`
    compression = 0x4, //!< `$4X01 <address> <value>` + `$<count> <address gap> <value gap>`
    mov = 0x5, //!< `$5X00 <destination> <source>`
    pointer_compression = 0x7, //!< a `$3` block plus the count and gaps of a `$4` code
    pointer_mov = 0x8, //!< `$8X<level> ... $88XX` (source) then `$8<4|5|6><level> ... $89XX` (destination)
    arm_write = 0xA, //!< `$AX00 <address> <instruction>`, restored when the cheat is turned off
    relative_base = 0xB, //!< `$B2<module> 0000000<segment> 00000000`
    button_pad = 0xC, //!< `$C2<lines> <pad type> <button mask>`
    condition = 0xD, //!< `$DX<lines> <address> <value>`
};

// Value width used by most code types, the second hexadecimal digit of the control word.
enum class CodeWidth : uint8_t {
    bits8 = 0,
    bits16 = 1,
    bits32 = 2,
};

// One `$XXXX AAAAAAAA BBBBBBBB` line.
struct CodeLine {
    uint16_t control = 0;
    uint32_t first = 0;
    uint32_t second = 0;

    CodeType type() const {
        return static_cast<CodeType>(control >> 12);
    }
    // Second digit: value width for most types, operator for conditions, direction for pointer MOV.
    uint8_t op() const {
        return (control >> 8) & 0xF;
    }
    // Last two digits: related line count, pointer level or block terminator depending on the type.
    uint8_t param() const {
        return control & 0xFF;
    }
};

// Guest memory saved before an ARM write ($A) patched it, to restore it when the cheat goes off.
struct SavedMemory {
    uint32_t address = 0;
    std::vector<uint8_t> bytes;
};

struct Cheat {
    std::string name;
    // `_V1`: the cheat is turned on automatically when the game boots.
    bool enabled_on_boot = false;
    bool enabled = false;
    std::vector<CodeLine> lines;
    bool broken = false;
    std::vector<SavedMemory> saved_memory;
    size_t line_number = 0;
};

struct CheatFile {
    fs::path path;
    std::string title_id;
    // First comment line of the file, usually `# <title id> <game name>`.
    std::string header;
    std::vector<Cheat> cheats;
};

fs::path find_cheat_file(const fs::path &cheats_dir, const std::string &title_id);

// Parse a cheat file. Malformed lines are logged and skipped, they never abort the parsing.
CheatFile parse_cheat_file(const fs::path &path, const std::string &title_id);

// Rewrite only the `_V0`/`_V1` markers of `file`, so that cheats on now are on again at next boot.
bool save_cheat_file(const CheatFile &file);

} // namespace cheat
