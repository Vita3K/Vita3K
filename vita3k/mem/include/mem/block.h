#pragma once

#include <functional>
#include <mem/util.h>

template <typename T>
class Ptr;

class Block {
public:
    typedef std::function<void(Address)> Deleter;

    explicit Block(Address addr, Deleter deleter)
        : addr(addr)
        , deleter(deleter) {
    }

    Block()
        : addr(0)
        , deleter(nullptr) {
    }

    Block(std::nullptr_t)
        : addr(0)
        , deleter(nullptr) {
    }

    Block &operator=(std::nullptr_t) {
        if (deleter) {
            deleter(addr);
        }
        addr = 0;
        deleter = nullptr;
        return *this;
    }

    ~Block() {
        if (deleter) {
            deleter(addr);
        }
    }

    operator bool() const {
        return addr != 0;
    }

    Address get() const {
        return addr;
    }

    template <class T>
    Ptr<T> get_ptr() const {
        return Ptr<T>(addr);
    }

    Block(Block &&o) noexcept {
        std::swap(addr, o.addr);
        std::swap(deleter, o.deleter);
    }

    Block &operator=(Block &&o) noexcept {
        std::swap(addr, o.addr);
        std::swap(deleter, o.deleter);
        return *this;
    }

private:
    Block(const Block &) = delete;
    const Block &operator=(const Block &) = delete;

    Address addr;
    Deleter deleter;
};