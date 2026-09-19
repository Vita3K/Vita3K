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

#include <mem/allocator.h>

#include <algorithm>
#include <bit>
#include <limits>

BitmapAllocator::BitmapAllocator(const std::size_t total_bits)
    : words((total_bits >> 5) + ((total_bits % 32 != 0) ? 1 : 0), 0xFFFFFFFF)
    , max_offset(total_bits) {
}

void BitmapAllocator::set_maximum(const std::size_t total_bits) {
    const std::size_t total_after = (total_bits >> 5) + ((total_bits % 32 != 0) ? 1 : 0);

    // words appended by the resize start out fully free
    words.resize(total_after, 0xFFFF'FFFFU);

    max_offset = total_bits;
}

void BitmapAllocator::reset() {
    words.clear();
}

int BitmapAllocator::force_fill(const std::uint32_t offset, const std::uint32_t size, const bool or_mode) {
    std::uint32_t *word = words.data() + (offset >> 5);
    const std::uint32_t set_bit = offset & 31;
    std::uint32_t end_bit = set_bit + size;

    if (end_bit <= 32) {
        // The bit we need to allocate is in single word
        const std::uint32_t mask = size == 32 ? 0xFFFFFFFFU : ((~(0xFFFFFFFFU >> size)) >> set_bit);

        if (or_mode) {
            *word |= mask;
        } else {
            *word &= ~mask;
        }

        return std::min<int>(size, (words.size() << 5) - set_bit);
    }

    // All the bits trails across some other words
    std::uint32_t mask = 0xFFFFFFFFU >> set_bit;

    while (end_bit > 0 && (word != words.data() + words.size())) {
        if (or_mode) {
            *word |= mask;
        } else {
            *word &= ~mask;
        }

        word += 1;
        if (end_bit < 32)
            break;
        // We only need to be careful with the first word, since it only fills
        // some first bits. We should fully fill with other word, so set the mask full
        mask = 0xFFFFFFFFU;
        end_bit -= 32;

        if (end_bit < 32) {
            mask = ~(mask >> end_bit);
        }
    }

    return std::min<int>(size, (words.size() << 5) - set_bit);
}

void BitmapAllocator::free(const std::uint32_t offset, const std::uint32_t size) {
    if (static_cast<std::size_t>(offset) >= max_offset) {
        return;
    }

    force_fill(offset, size, true);
}

int BitmapAllocator::allocate_from(const std::uint32_t start_offset, std::uint32_t &size, const bool best_fit) {
    if (words.empty()) {
        return -1;
    }

    // the tail bits of the last word are set, but the slots past max_offset don't exist
    const std::uint32_t total_bits = static_cast<std::uint32_t>(std::min(words.size() << 5, max_offset));

    // start scanning at the word holding start_offset, a free slot is a set bit
    std::uint32_t cursor = start_offset & ~31U;

    if (cursor >= total_bits) {
        return -1;
    }

    std::uint32_t best_offset = 0;
    std::uint32_t best_length = std::numeric_limits<std::uint32_t>::max();

    // Slots are numbered from the most significant bit, so a run of free slots is a
    // run of leading ones once the already visited bits have been shifted out.
    const auto visible_from = [this](const std::uint32_t bit) {
        return static_cast<std::uint32_t>(words[bit >> 5] << (bit & 31));
    };

    while (cursor < total_bits) {
        const std::uint32_t remaining = visible_from(cursor);

        if (remaining == 0) {
            // nothing free left in this word
            cursor += 32 - (cursor & 31);
            continue;
        }

        // jump over the allocated slots, then measure the free run starting there
        cursor += std::countl_zero(remaining);
        if (cursor >= total_bits) {
            break;
        }

        const std::uint32_t offset = cursor;
        std::uint32_t length = 0;

        while (cursor < total_bits) {
            const std::uint32_t run = std::min<std::uint32_t>(std::countl_one(visible_from(cursor)), total_bits - cursor);
            if (run == 0) {
                break;
            }

            length += run;
            cursor += run;

            // the run ended on an allocated slot rather than on a word boundary
            if ((cursor & 31) != 0) {
                break;
            }
        }

        if (length >= size) {
            if (!best_fit) {
                // Force allocate and then return
                size = force_fill(offset, size, false);
                return static_cast<int>(offset);
            } else if (length < best_length) {
                best_length = length;
                best_offset = offset;
            }
        }
    }

    if (best_fit && best_length != std::numeric_limits<std::uint32_t>::max()) {
        // Force allocate and then return
        size = force_fill(best_offset, size, false);
        return static_cast<int>(best_offset);
    }

    return -1;
}

int BitmapAllocator::allocate_at(const std::uint32_t start_offset, std::uint32_t size) {
    if (free_slot_count(start_offset, start_offset + size) != size) {
        return -1;
    }

    force_fill(start_offset, size, false);
    return 0;
}

int BitmapAllocator::free_slot_count(const std::uint32_t offset, const std::uint32_t offset_end) const {
    if (offset >= offset_end) {
        return -1;
    }

    const std::uint32_t beg_off = (offset >> 5);
    const std::uint32_t end_off = (offset_end >> 5);

    if (beg_off >= words.size()) {
        return -1;
    }

    std::uint32_t start_bit = offset;
    const std::uint32_t end_bit = end_off >= words.size() ? max_offset : offset_end;

    std::uint32_t free_count = 0;

    while (start_bit < end_bit) {
        const std::uint32_t next_end_bit = std::min<std::uint32_t>(((start_bit + 32) >> 5) << 5, end_bit);

        const int left_shift = start_bit & 31;
        const int right_shift = (31 - (next_end_bit - 1) & 31);
        const std::uint32_t word_to_scan = words[start_bit >> 5] << left_shift >> right_shift >> left_shift;
        free_count += std::popcount(word_to_scan);

        start_bit = next_end_bit;
    }

    return free_count;
}
