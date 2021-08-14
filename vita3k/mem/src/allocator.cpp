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

#include <mem/allocator.h>

BitmapAllocator::BitmapAllocator(const std::size_t total_bits)
    : words((total_bits >> 5) + ((total_bits % 32 != 0) ? 1 : 0), 0xFFFFFFFF)
    , max_offset(total_bits) {
}

void BitmapAllocator::set_maximum(const std::size_t total_bits) {
    const std::size_t total_before = words.size();
    const std::size_t total_after = (total_bits >> 5) + ((total_bits % 32 != 0) ? 1 : 0);

    words.resize(total_after);

    if (total_after > total_before) {
        for (std::size_t i = total_before; i < total_after; i++) {
            words[i] = 0xFFFFFFFFU;
        }
    }

    max_offset = total_bits;
}

void BitmapAllocator::reset() {
    words.clear();
}

int BitmapAllocator::force_fill(const std::uint32_t offset, const int size, const bool or_mode) {
    std::uint32_t *word = &words[0] + (offset >> 5);
    const std::uint32_t set_bit = offset & 31;
    int end_bit = static_cast<int>(set_bit + size);

    std::uint32_t wval = *word;

    if (end_bit <= 32) {
        // The bit we need to allocate is in single word
        const std::uint32_t mask = size == 32 ? 0xFFFFFFFFU : ((~(0xFFFFFFFFU >> size)) >> set_bit);

        if (or_mode) {
            *word = wval | mask;
        } else {
            *word = wval & (~mask);
        }

        return std::min<int>(size, static_cast<int>((words.size() << 5) - set_bit));
    }

    // All the bits trails across some other words
    std::uint32_t mask = 0xFFFFFFFFU >> set_bit;

    while (end_bit > 0 && (word != words.data() + words.size())) {
        wval = *word;

        if (or_mode) {
            *word = wval | mask;
        } else {
            *word = wval & (~mask);
        }

        word += 1;

        // We only need to be careful with the first word, since it only fills
        // some first bits. We should fully fill with other word, so set the mask full
        mask = 0xFFFFFFFFU;
        end_bit -= 32;

        if (end_bit < 32) {
            mask = ~(mask >> static_cast<std::uint32_t>(end_bit));
        }
    }

    return std::min<int>(size, static_cast<int>((words.size() << 5) - set_bit));
}

void BitmapAllocator::free(const std::uint32_t offset, const int size) {
    if (static_cast<std::size_t>(offset) >= max_offset) {
        return;
    }

    force_fill(offset, size, true);
}

int BitmapAllocator::allocate_from(const std::uint32_t start_offset, int &size, const bool best_fit) {
    if (words.empty()) {
        return -1;
    }

    std::uint32_t *word = &words[0] + (start_offset >> 5);

    // We have arrived at le word that still have free position (bit 1)
    std::uint32_t *word_end = &words[words.size() - 1];
    std::uint32_t soc = start_offset;

    int bflmin = 0xFFFFFF;
    int bofmin = -1;
    std::uint32_t *wordmin = nullptr;

    // Keep finding
    while (word <= word_end) {
        std::uint32_t wv = *word;

        if (wv != 0) {
            // Still have free stuff
            int bflen = 0;
            int boff = 0;
            std::uint32_t *bword = nullptr;

            int cursor = 31;

            while (cursor >= 0) {
                if (((wv >> cursor) & 1) == 1) {
                    boff = cursor;
                    bflen = 0;
                    bword = word;

                    while (cursor >= 0 && (((wv >> cursor) & 1) == 1)) {
                        bflen++;
                        cursor--;

                        if (cursor < 0 && (word + 1 <= word_end)) {
                            cursor = 31;
                            soc = 32;
                            word++;
                            wv = *word;
                        }
                    }

                    if (bflen >= size) {
                        if (!best_fit) {
                            // Force allocate and then return
                            const int offset = static_cast<int>(31 - boff + ((bword - &words[0]) << 5));
                            if (static_cast<std::size_t>(offset + size) <= max_offset) {
                                size = force_fill(static_cast<const std::uint32_t>(offset), size, false);
                                return offset;
                            }
                        } else {
                            if (bflen < bflmin) {
                                bflmin = bflen;
                                bofmin = boff;
                                wordmin = bword;
                            }
                        }
                    }
                }

                cursor--;
            }
        }

        soc = 32;
        word++;
    }

    if (best_fit && bofmin != -1) {
        // Force allocate and then return
        const int offset = static_cast<int>(31 - bofmin + ((wordmin - &words[0]) << 5));
        if (static_cast<std::size_t>(offset + size) <= max_offset) {
            size = force_fill(static_cast<std::uint32_t>(offset), size, false);
            return offset;
        }
    }

    return -1;
}

int BitmapAllocator::allocate_at(const std::uint32_t start_offset, int size) {
    if (free_slot_count(start_offset, start_offset + size) != size) {
        return -1;
    }

    force_fill(start_offset, size, false);
    return 0;
}

static int number_of_set_bits(std::uint32_t i) {
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
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
        std::uint32_t word_to_scan = words[start_bit >> 5] << left_shift >> right_shift >> left_shift;
        free_count += number_of_set_bits(word_to_scan);

        start_bit = next_end_bit;
    }

    return free_count;
}
