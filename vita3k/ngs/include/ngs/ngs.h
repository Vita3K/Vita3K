#pragma once

#include <cstdint>
#include <vector>

#include <mem/ptr.h>

namespace emu::ngs {
    struct SystemInitParameters {
        std::int32_t max_racks;
        std::int32_t max_voices;
        std::int32_t granularity;
        std::int32_t sample_rate;
        std::int32_t unk16;
    };

    struct System;

    struct Rack {
        System *mama;
        Address my_location;
    };

    struct SystemMemspaceBlockAllocator {
        struct Block {
            std::uint32_t size;
            std::uint32_t offset;
            bool free;
        };

        std::vector<Block> blocks;      /// All block sorted by size

        explicit SystemMemspaceBlockAllocator(const std::uint32_t memspace_size);
        
        std::uint32_t alloc(const std::uint32_t size);
        bool free(const std::uint32_t offset);
    };

    struct System {
        std::vector<Rack*> racks;
        std::int32_t max_voices;
        std::int32_t granularity;
        std::int32_t sample_rate;

        Ptr<void> memspace;
    };
}