#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

struct BitmapAllocator {
    std::vector<std::uint32_t> words;
    std::size_t max_offset;

protected:
    int force_fill(const std::uint32_t offset, const int size, const bool or_mode = false);

public:
    BitmapAllocator() = default;
    explicit BitmapAllocator(const std::size_t total_bits);

    void set_maximum(const std::size_t total_bits);

    int allocate_from(const std::uint32_t start_offset, int &size, const bool best_fit = false);
    int allocate_at(const std::uint32_t start_offset, int size);
    void free(const std::uint32_t offset, const int size);
    void reset();

    // Count free bits in [offset, offset_end) (exclusive)
    int free_slot_count(const std::uint32_t offset, const std::uint32_t offset_end) const;
};
