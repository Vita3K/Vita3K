#pragma once

#include <cstring>
#include <memory>

#if WIN32
#include <intrin.h>
#endif

#if WIN32

inline bool atomic_compare_and_swap(volatile uint8_t *pointer, uint8_t value, uint8_t expected) {
    const uint8_t result = _InterlockedCompareExchange8(reinterpret_cast<volatile char *>(pointer), value, expected);
    return result == expected;
}

inline bool atomic_compare_and_swap(volatile uint16_t *pointer, uint16_t value, uint16_t expected) {
    const uint16_t result = _InterlockedCompareExchange16(reinterpret_cast<volatile short *>(pointer), value, expected);
    return result == expected;
}

inline bool atomic_compare_and_swap(volatile uint32_t *pointer, uint32_t value, uint32_t expected) {
    const uint32_t result = _InterlockedCompareExchange(reinterpret_cast<volatile long *>(pointer), value, expected);
    return result == expected;
}

inline bool atomic_compare_and_swap(volatile uint64_t *pointer, uint64_t value, uint64_t expected) {
    const uint64_t result = _InterlockedCompareExchange64(reinterpret_cast<volatile __int64 *>(pointer),
        value, expected);
    return result == expected;
}

inline uint32_t atomic_load_acquire(volatile uint32_t *pointer) {
    return *pointer;
}

inline void atomic_store_release(volatile uint32_t *pointer, uint32_t value) {
    *pointer = value;
}

#else

inline bool atomic_compare_and_swap(volatile uint8_t *pointer, uint8_t value, uint8_t expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

inline bool atomic_compare_and_swap(volatile uint16_t *pointer, uint16_t value, uint16_t expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

inline bool atomic_compare_and_swap(volatile uint32_t *pointer, uint32_t value, uint32_t expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

inline bool atomic_compare_and_swap(volatile uint64_t *pointer, uint64_t value, uint64_t expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

inline uint32_t atomic_load_acquire(volatile uint32_t *pointer) {
    uint32_t value;
    __atomic_load(pointer, &value, __ATOMIC_ACQUIRE);
    return value;
}

inline void atomic_store_release(volatile uint32_t *pointer, uint32_t value) {
    __atomic_store(pointer, &value, __ATOMIC_RELEASE);
}

#endif
