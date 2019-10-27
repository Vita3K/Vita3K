#include <ngs/ngs.h>

namespace emu::ngs {
    SystemMemspaceBlockAllocator::SystemMemspaceBlockAllocator(const std::uint32_t memspace_size) {
        Block main_block;
        main_block.offset = 0;
        main_block.size = memspace_size;

        blocks.push_back(main_block);
    }
    
    std::uint32_t SystemMemspaceBlockAllocator::alloc(const std::uint32_t size) {
        for (std::size_t i = 0; i < blocks.size(); i++) {
            if (blocks[i].free && blocks[i].size >= size) {
                // Case 1: Equals. Assign it and returns
                if (blocks[i].size == size) {
                    blocks[i].free = false;
                    return blocks[i].offset;
                } else {
                    // Create new block
                    Block division;
                    division.offset = blocks[i].offset + size;
                    division.size = blocks[i].size - size;
                    division.free = true;

                    blocks[i].free = false;
                    blocks[i].size = size;

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

    bool SystemMemspaceBlockAllocator::free(const std::uint32_t offset) {
        auto ite = std::lower_bound(blocks.begin(), blocks.end(), offset, [](const Block &lhs, const std::uint32_t rhs) {
            return lhs.offset < rhs;
        });

        if (ite != blocks.end() && ite->offset == offset) {
            ite->free = true;
            return true;
        }

        return false;
    }
}