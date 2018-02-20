#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#ifdef _MSC_VER
#define ASSUME(cond) __assume(cond)
#define LIKELY
#define UNLIKELY
#define SAFE_BUFFERS __declspec(safebuffers)
#define NEVER_INLINE __declspec(noinline)
#define FORCE_INLINE __forceinline
#else
#define ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
#define LIKELY(cond) __builtin_expect(!!(cond), 1)
#define UNLIKELY(cond) __builtin_expect(!!(cond), 0)
#define SAFE_BUFFERS
#define NEVER_INLINE __attribute__((noinline))
#define FORCE_INLINE __attribute__((always_inline)) inline
#endif

#define CHECK_SIZE(type, size) static_assert(sizeof(type) == size, "Invalid " #type " type size")
#define CHECK_ALIGN(type, align) static_assert(alignof(type) == align, "Invalid " #type " type alignment")
#define CHECK_MAX_SIZE(type, size) static_assert(sizeof(type) <= size, #type " type size is too big")
#define CHECK_SIZE_ALIGN(type, size, align) CHECK_SIZE(type, size); CHECK_ALIGN(type, align)


// Return 32 bit sizeof() to avoid widening/narrowing conversions with size_t
#define SIZE_32(...) static_cast<u32>(sizeof(__VA_ARGS__))

// Return 32 bit alignof() to avoid widening/narrowing conversions with size_t
#define ALIGN_32(...) static_cast<u32>(alignof(__VA_ARGS__))

#define CONCATENATE_DETAIL(x, y) x ## y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)

#define STRINGIZE_DETAIL(x) #x ""
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define HERE "\n(in file " __FILE__ ":" STRINGIZE(__LINE__) ")"

using schar = signed char;
using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using ulong = unsigned long;
using ullong = unsigned long long;
using llong = long long;

using uptr = std::uintptr_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

using f32 = float;
using f64 = double;

using size_t = std::size_t;

// Helper function, used by ""_u16, ""_u32, ""_u64
constexpr u8 to_u8(char c)
{
    return static_cast<u8>(c);
}

// Convert 2-byte string to u16 value like reinterpret_cast does
constexpr u16 operator""_u16(const char* s, std::size_t length)
{
    return length != 2 ? throw s :
        to_u8(s[1]) << 8 | to_u8(s[0]);
}

// Convert 4-byte string to u32 value like reinterpret_cast does
constexpr u32 operator""_u32(const char* s, std::size_t length)
{
    return length != 4 ? throw s :
        to_u8(s[3]) << 24 | to_u8(s[2]) << 16 | to_u8(s[1]) << 8 | to_u8(s[0]);
}

// Convert 8-byte string to u64 value like reinterpret_cast does
constexpr u64 operator""_u64(const char* s, std::size_t length)
{
    return length != 8 ? throw s :
        static_cast<u64>(to_u8(s[7]) << 24 | to_u8(s[6]) << 16 | to_u8(s[5]) << 8 | to_u8(s[4])) << 32 | to_u8(s[3]) << 24 | to_u8(s[2]) << 16 | to_u8(s[1]) << 8 | to_u8(s[0]);
}

using Buffer = std::vector<u8>;
