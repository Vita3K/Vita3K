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
    uint8_t is_short : 4;
    uint8_t symbol_segment : 4;
    uint8_t code;
};

struct ShortEntry : Entry {
    uint16_t data_segment : 4;
    uint16_t offset_lo : 12;
    uint32_t offset_hi : 20;
    uint32_t addend : 12;
};

struct LongEntry : Entry {
    uint16_t data_segment : 4;
    uint16_t code2 : 8;
    uint16_t dist2 : 4;
    uint32_t addend;
    uint32_t offset;
};

static_assert(sizeof(ShortEntry) == 8, "Short entry has incorrect size.");
static_assert(sizeof(LongEntry) == 12, "Long entry has incorrect size.");

static void write(void *data, uint32_t value) {
    memcpy(data, &value, sizeof(value));
}

static void write_masked(void *data, uint32_t symbol, uint32_t mask) {
    write(data, symbol & mask);
}

static void write_thumb_call(void *data, uint32_t symbol) {
    // This is cribbed from UVLoader, but I used bitfields to get rid of some shifting and masking.
    struct Upper {
        uint16_t imm10 : 10;
        uint16_t sign : 1;
        uint16_t ignored : 5;
    };

    struct Lower {
        uint16_t imm11 : 11;
        uint16_t j2 : 1;
        uint16_t unknown : 1;
        uint16_t j1 : 1;
        uint16_t unknown2 : 2;
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

static void write_mov_abs(void *data, uint16_t symbol) {
    struct Instruction {
        uint32_t imm12 : 12;
        uint32_t ignored1 : 4;
        uint32_t imm4 : 4;
        uint32_t ignored2 : 12;
    };

    static_assert(sizeof(Instruction) == 4, "Incorrect size.");

    Instruction *const instruction = static_cast<Instruction *>(data);
    instruction->imm12 = symbol;
    instruction->imm4 = symbol >> 12;
}

static void write_thumb_mov_abs(void *data, uint16_t symbol) {
    // This is cribbed from UVLoader, but I used bitfields to get rid of some shifting and masking.
    struct Upper {
        uint16_t imm4 : 4;
        uint16_t ignored1 : 6;
        uint16_t i : 1;
        uint16_t ignored2 : 5;
    };

    struct Lower {
        uint16_t imm8 : 8;
        uint16_t ignored1 : 4;
        uint16_t imm3 : 3;
        uint16_t ignored2 : 1;
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

static bool relocate(void *data, Code code, uint32_t s, uint32_t a, uint32_t p) {
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
    const void *const end = static_cast<const uint8_t *>(entries) + size;
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
            if (!relocate(Ptr<uint32_t>(p).get(mem), static_cast<Code>(entry->code), s, a, p)) {
                return false;
            }

            entry = long_entry + 1;
        }
    }

    return true;
}
