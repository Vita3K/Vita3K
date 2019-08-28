#pragma once

#include <cstddef>

constexpr size_t align(size_t current, size_t alignment) {
    return (current + (alignment - 1)) & ~(alignment - 1);
}
