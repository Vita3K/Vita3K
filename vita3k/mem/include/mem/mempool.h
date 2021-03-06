#pragma once

#include <mem/ptr.h>
#include <util/align.h>

#include <mutex>

struct MemspaceBlockAllocator {
    struct Block {
        std::uint32_t size;
        std::uint32_t offset;
        bool free;
    };

    std::vector<Block> blocks; /// All block sorted by size
    std::mutex lock;

    explicit MemspaceBlockAllocator() = default;
    explicit MemspaceBlockAllocator(const std::uint32_t memspace_size) {
        init(memspace_size);
    }

    void init(const std::uint32_t memspace_size) {
        Block main_block = {};
        main_block.offset = 0;
        main_block.size = memspace_size;
        main_block.free = true;

        blocks.push_back(main_block);
    }

    std::uint32_t alloc(const std::uint32_t size) {
        std::uint32_t aligned_size = static_cast<std::uint32_t>(align(size, 4));
        const std::lock_guard<std::mutex> guard(lock);

        for (std::size_t i = 0; i < blocks.size(); i++) {
            if (blocks[i].free && blocks[i].size >= aligned_size) {
                // Case 1: Equals. Assign it and returns
                if (blocks[i].size == aligned_size) {
                    blocks[i].free = false;
                    return blocks[i].offset;
                } else {
                    // Create new block
                    Block division;
                    division.offset = blocks[i].offset + aligned_size;
                    division.size = blocks[i].size - aligned_size;
                    division.free = true;

                    blocks[i].free = false;
                    blocks[i].size = aligned_size;

                    const std::uint32_t offset = blocks[i].offset;

                    if (blocks.capacity() == blocks.size()) {
                        blocks.resize(blocks.size() + 1);
                    }

                    blocks.insert(blocks.begin() + i + 1, division);

                    return offset;
                }
            }
        }

        return 0xFFFFFFFF;
    }

    bool free(const std::uint32_t offset) {
        const std::lock_guard<std::mutex> guard(lock);

        auto ite = std::lower_bound(blocks.begin(), blocks.end(), offset, [](const Block &lhs, const std::uint32_t rhs) {
            return lhs.offset < rhs;
        });

        if (ite != blocks.end() && ite->offset == offset) {
            ite->free = true;
            return true;
        }

        return false;
    }
};

struct MempoolObject {
    Ptr<void> memspace;
    MemspaceBlockAllocator allocator;

    explicit MempoolObject(const Ptr<void> memspace, const std::uint32_t memspace_size)
        : memspace(memspace)
        , allocator(memspace_size) {
    }
    explicit MempoolObject() = default;

    Ptr<void> alloc_raw(const std::uint32_t size) {
        const std::uint32_t offset = allocator.alloc(size);

        if (offset == 0xFFFFFFFF) {
            return Ptr<void>();
        }

        return Ptr<void>(memspace.address() + offset);
    }

    bool free_raw(const Ptr<void> ptr) {
        if (ptr < memspace) {
            return false;
        }

        return allocator.free(ptr.address() - memspace.address());
    }

    template <typename T>
    Ptr<T> alloc() {
        return alloc_raw(sizeof(T)).cast<T>();
    }

    template <typename T, typename... Args>
    Ptr<T> alloc_and_init(const MemState &mem, Args... args) {
        Ptr<T> result = alloc_raw(sizeof(T)).cast<T>();
        T *ptr = result.get(mem);
        ptr = new (ptr) T(args...);
        return result;
    }

    template <typename T>
    bool free(const Ptr<T> ptr) {
        return free_raw(ptr.template cast<void>());
    }
};