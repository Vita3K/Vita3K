#pragma once

#include <algorithm>
#include <atomic>
#include <memory>

#include <cassert>

//Ring buffer for bytes - not multi-thread safe, bring your own locks
class ByteRingBuffer {
public:
    ByteRingBuffer(std::size_t size)
        : buffer(new char[size])
        , capacity(size){

        };

    bool Empty() const { return (used == 0); }
    bool Full() const { return (used == capacity); }
    std::size_t Capacity() const { return capacity; }
    std::size_t Free() const { return capacity - used; }
    std::size_t Used() const { return used; }

    std::size_t Insert(const void *in, std::size_t size) {
        if (Full()) {
            return 0;
        }

        const std::size_t insertSize = std::min(size, Free()); //Biggest we can insert in buffer
        if (start <= end) { //There is space between 'end' and buffer end, and potentially between 0 and 'start'
            const std::size_t bufEndInsertSize = std::min(capacity - end, insertSize); //Size of data to put between 'end' and buffer end
            const std::size_t bufStartInsertSize = std::min(start, insertSize - bufEndInsertSize); //Size of data to put between 0 and start

            assert((bufEndInsertSize + bufStartInsertSize) == insertSize);

            memcpy(&buffer[end], static_cast<const char *>(in), bufEndInsertSize);
            memcpy(&buffer[0], static_cast<const char *>(in) + bufEndInsertSize, bufStartInsertSize);
        } else { //There is space between 'end' and 'start'
            memcpy(&buffer[end], static_cast<const char *>(in), insertSize);
        }

        end = (end + insertSize) % capacity;
        used += insertSize;
        return insertSize;
    }

    std::size_t Remove(void *out, std::size_t size) {
        std::size_t removed = Peek(out, size); //If empty, Peek should copy 0
        start = (start + removed) % capacity;
        used -= removed;
        return removed;
    }

    std::size_t Peek(void *out, std::size_t size) const {
        if (Empty()) {
            return 0;
        }

        const std::size_t extractSize = std::min(size, Used());
        if (start < end) { //Data between 'start' and 'end'-1
            memcpy(out, &buffer[start], extractSize);
        } else { //Data between 'start' and buffer end, and potentially between 0 and 'end'-1
            const std::size_t bufTailSize = std::min(capacity - start, extractSize); //Size of stuff to copy between 'start' and buffer end
            const std::size_t bufHeadSize = std::min(end, extractSize - bufTailSize); //Size of stuff to copy at buffer start

            assert((bufTailSize + bufHeadSize) == extractSize);
            memcpy(out, &buffer[start], bufTailSize);
            memcpy(static_cast<char *>(out) + bufTailSize, &buffer[0], bufHeadSize);
        }
        return extractSize;
    }

private:
    std::unique_ptr<char[]> buffer;
    const std::size_t capacity;

    //buffer[start] -> buffer[end - 1] is used
    std::size_t start = 0;
    std::size_t end = 0;
    std::size_t used = 0;
};
