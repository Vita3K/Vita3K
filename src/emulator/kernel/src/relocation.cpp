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

#include <kernel/relocation.h>
#include <util/log.h>

#include <cassert>
#include <cstring>
#include <iostream>

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
    uint32_t offset_hi : 10;
    uint32_t addend : 22;
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
        const auto symbol_start_it = segments.find(entry->symbol_segment);

        if (symbol_start_it != segments.end()) {
            const Ptr<void> symbol_start = symbol_start_it->second;

            const Address s = (entry->symbol_segment == 0xf) ? 0 : symbol_start.address();

            if (entry->is_short) {
                const ShortEntry *const short_entry = static_cast<const ShortEntry *>(entry);
                const auto data_segment_it = segments.find(short_entry->data_segment);

                if (data_segment_it != segments.end()) {
                    const Ptr<void> data_segment = data_segment_it->second;
                    const Address offset = short_entry->offset_lo | (short_entry->offset_hi << 12);
                    const Address p = data_segment.address() + offset;
                    const Address a = short_entry->addend;
                    if (!relocate(Ptr<uint32_t>(p).get(mem), static_cast<Code>(entry->code), s, a, p)) {
                        return false;
                    }
                } else {
                    LOG_WARN("Segment {} not found for short relocation with code {}, skipping", short_entry->data_segment, entry->code);
                }
            } else {
                const LongEntry *const long_entry = static_cast<const LongEntry *>(entry);

                const auto data_segment_it = segments.find(long_entry->data_segment);

                if (data_segment_it != segments.end()) {
                    const Ptr<void> data_segment = data_segment_it->second;
                    const Address p = data_segment.address() + long_entry->offset;
                    const Address a = long_entry->addend;
                    if (!relocate(Ptr<uint32_t>(p).get(mem), static_cast<Code>(entry->code), s, a, p)) {
                        return false;
                    }

                    if (long_entry->code2 != 0) {
                        if (!relocate(Ptr<uint32_t>(p + (long_entry->dist2 * 2)).get(mem), static_cast<Code>(long_entry->code2), s, a, p)) {
                            return false;
                        }
                    }
                } else {
                    LOG_WARN("Segment {} not found for long relocation with code {}, skipping", long_entry->data_segment, entry->code);
                }
            }
        } else {
            LOG_WARN("Symbol segment {} not found for relocation with code {}, skipping", entry->symbol_segment, entry->code);
        }

        if (entry->is_short)
            entry = static_cast<const ShortEntry *>(entry) + 1;
        else
            entry = static_cast<const LongEntry *>(entry) + 1;
    }

    return true;
}
