#pragma once

#define UNWRAP(x) x

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)

#define STRINGIZE_DETAIL(x) #x ""
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define STR_CASE(...) \
    case __VA_ARGS__: \
        return #__VA_ARGS__

#ifdef WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#endif

// Based on: https://stackoverflow.com/a/52379410
#ifdef COMPILE_MODERN_CPP
#ifdef CPP17_SUPPORTED
#define VITA3K_CPP17 1
#elif CPP14_SUPPORTED
#define VITA3K_CPP14 1
#endif
#endif

#if VITA3K_CPP17
#define VITA3K_IF_CONSTEXPR if constexpr
#define VITA3K_INLINE_CONSTEXPR inline constexpr
#else
#define VITA3K_IF_CONSTEXPR if
#define VITA3K_INLINE_CONSTEXPR constexpr
#endif
