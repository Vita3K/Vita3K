#pragma once

#include <cstdint>
#include <vector>

#include <mem/ptr.h>

namespace ngs {
    struct MemspaceBlockAllocator {
        struct Block {
            std::uint32_t size;
            std::uint32_t offset;
            bool free;
        };

        std::vector<Block> blocks;      /// All block sorted by size

        explicit MemspaceBlockAllocator() = default;
        explicit MemspaceBlockAllocator(const std::uint32_t memspace_size);
        
        void init(const std::uint32_t memspace_size);

        std::uint32_t alloc(const std::uint32_t size);
        bool free(const std::uint32_t offset);
    };

    struct MempoolObject {
        Ptr<void> memspace;
        MemspaceBlockAllocator allocator;

        explicit MempoolObject(const Ptr<void> memspace, const std::uint32_t memspace_size);
        explicit MempoolObject() = default;

        Ptr<void> alloc_raw(const std::uint32_t size);
        bool free_raw(const Ptr<void> ptr);

        template <typename T>
        Ptr<T> alloc() {
            return alloc_raw(sizeof(T)).cast<T>();
        }

        template <typename T, typename ...Args>
        Ptr<T> alloc_and_init(const MemState &mem, Args... args) {
            Ptr<T> result = alloc_raw(sizeof(T)).cast<T>();
            T *ptr = result.get(mem);
            ptr = new (ptr) T (args...);
            return result;
        }

        template <typename T>
        bool free(const Ptr<T> ptr) {
            return free_raw(ptr.template cast<void>());
        }
    };    
}