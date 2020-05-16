#pragma once

#include <cstdint>
#include <type_traits>

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
constexpr T align(const T &value, std::uint64_t align) {
    return static_cast<T>((value + (align - 1)) & ~(align - 1));
}

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
constexpr T align_down(const T &value, std::uint64_t align) {
    return static_cast<T>(value & ~(align - 1));
}

std::uint32_t nearest_power_of_two(std::uint32_t num);