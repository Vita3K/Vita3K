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

#include "relocation.h"
#include <util/log.h>
#include <util/types.h>

#include <assert.h>
#include <iostream>
#include <cstring>

enum Code {
    None = 0,
    Abs32 = 2,
    Rel32 = 3,
    ThumbCall = 10,
    Call = 28,
    Jump24 = 29,
    Target1 = 38,
    V4BX = 40,
    Target2 = 41,
    Prel31 = 42,
    MovwAbsNc = 43,
    MovtAbs = 44,
    ThumbMovwAbsNc = 47,
    ThumbMovtAbs = 48
};

struct Entry {
    u8 is_short : 4;
    u8 symbol_segment : 4;
    u8 code;
};

struct ShortEntry : Entry {
    u16 data_segment : 4;
    u16 offset_lo : 12;
    u32 offset_hi : 20;
    u32 addend : 12;
};

struct LongEntry : Entry {
    u16 data_segment : 4;
    u16 code2 : 8;
    u16 dist2 : 4;
    u32 addend;
    u32 offset;
};

static_assert(sizeof(ShortEntry) == 8, "Short entry has incorrect size.");
static_assert(sizeof(LongEntry) == 12, "Long entry has incorrect size.");

static void write(void *data, u32 value) {
    memcpy(data, &value, sizeof(value));
}

static void write_masked(void *data, u32 symbol, u32 mask) {
    write(data, symbol & mask);
}

static void write_thumb_call(void *data, u32 symbol) {
    // This is cribbed from UVLoader, but I used bitfields to get rid of some shifting and masking.
    struct Upper {
        u16 imm10 : 10;
        u16 sign : 1;
        u16 ignored : 5;
    };

    struct Lower {
        u16 imm11 : 11;
        u16 j2 : 1;
        u16 unknown : 1;
        u16 j1 : 1;
        u16 unknown2 : 2;
    };

    struct Pair {
        Upper upper;
        Lower lower;
    };

    static_assert(sizeof(Pair) == 4, "Incorrect size.");

    Pair *const pair = static_cast<Pair *>(data);
    pair->lower.imm11 = symbol >> 1;
    pair->upper.imm10 = symbol >> 12;
    pair->upper.sign = symbol >> 24;
    pair->lower.j2 = pair->upper.sign ^ ((~symbol) >> 22);
    pair->lower.j1 = pair->upper.sign ^ ((~symbol) >> 23);
}

static void write_mov_abs(void *data, u16 symbol) {
    struct Instruction {
        u32 imm12 : 12;
        u32 ignored1 : 4;
        u32 imm4 : 4;
        u32 ignored2 : 12;
    };

    static_assert(sizeof(Instruction) == 4, "Incorrect size.");

    Instruction *const instruction = static_cast<Instruction *>(data);
    instruction->imm12 = symbol;
    instruction->imm4 = symbol >> 12;
}

static void write_thumb_mov_abs(void *data, u16 symbol) {
    // This is cribbed from UVLoader, but I used bitfields to get rid of some shifting and masking.
    struct Upper {
        u16 imm4 : 4;
        u16 ignored1 : 6;
        u16 i : 1;
        u16 ignored2 : 5;
    };

    struct Lower {
        u16 imm8 : 8;
        u16 ignored1 : 4;
        u16 imm3 : 3;
        u16 ignored2 : 1;
    };

    struct Pair {
        Upper upper;
        Lower lower;
    };

    static_assert(sizeof(Pair) == 4, "Incorrect size.");

    Pair *const pair = static_cast<Pair *>(data);
    pair->lower.imm8 = symbol;
    pair->lower.imm3 = symbol >> 8;
    pair->upper.i = symbol >> 11;
    pair->upper.imm4 = symbol >> 12;
}

static bool relocate(void *data, Code code, u32 s, u32 a, u32 p) {
    switch (code) {
    case None:
    case V4BX: // Untested.
        return true;

    case Abs32:
    case Target1:
        write(data, s + a);
        return true;

    case Rel32:
    case Target2:
        write(data, s + a - p);
        return true;

    case Prel31:
        write_masked(data, s + a - p, INT32_MAX);
        return true;

    case ThumbCall:
        write_thumb_call(data, s + a - p);
        return true;

    case Call:
    case Jump24:
        write_masked(data, (s + a - p) >> 2, 0xffffff);
        return true;

    case MovwAbsNc:
        write_mov_abs(data, s + a);
        return true;

    case MovtAbs:
        write_mov_abs(data, (s + a) >> 16);
        return true;

    case ThumbMovwAbsNc:
        write_thumb_mov_abs(data, s + a);
        return true;

    case ThumbMovtAbs:
        write_thumb_mov_abs(data, (s + a) >> 16);
        return true;
    }

    LOG_WARN("Unhandled relocation code {}.", code);

    return true;
}

bool relocate(const void *entries, size_t size, const SegmentAddresses &segments, const MemState &mem) {
    const void *const end = static_cast<const u8 *>(entries) + size;
    const Entry *entry = static_cast<const Entry *>(entries);
    while (entry < end) {
        assert(entry->is_short == 0);

        const Ptr<void> symbol_start = segments.find(entry->symbol_segment)->second;
        const Address s = (entry->symbol_segment == 0xf) ? 0 : symbol_start.address();

        if (entry->is_short) {
            const ShortEntry *const short_entry = static_cast<const ShortEntry *>(entry);
            entry = short_entry + 1;
        } else {
            const LongEntry *const long_entry = static_cast<const LongEntry *>(entry);
            assert(long_entry->code2 == 0);

            const Ptr<void> segment_start = segments.find(long_entry->data_segment)->second;
            const Address p = segment_start.address() + long_entry->offset;
            const Address a = long_entry->addend;
            if (!relocate(Ptr<u32>(p).get(mem), static_cast<Code>(entry->code), s, a, p)) {
                return false;
            }

            entry = long_entry + 1;
        }
    }

    return true;
}
