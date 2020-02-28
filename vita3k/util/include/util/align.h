#pragma once

#include <cstdint>
#include <type_traits>

template <typename T, typename std::enable_if<std::is_integral<T>::value>::type * = nullptr>
constexpr T align(const T &value, std::uint64_t align) {
    return static_cast<T>((value + (align - 1)) & ~(align - 1));
}

template <typename T, typename std::enable_if<std::is_integral<T>::value>::type * = nullptr>
constexpr T align_down(const T &value, std::uint64_t align) {
    return static_cast<T>(value & ~(align - 1));
}
