#pragma once

#define UNWRAP(x) x

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)

#define STRINGIZE_DETAIL(x) #x ""
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define STR_CASE(...) \
    case __VA_ARGS__: \
        return #__VA_ARGS__

#if VITA3K_CPP17
#define VITA3K_IF_CONSTEXPR if constexpr
#define VITA3K_INLINE_CONSTEXPR inline constexpr
#else
#define VITA3K_IF_CONSTEXPR if
#define VITA3K_INLINE_CONSTEXPR constexpr
#endif

#if VITA3K_CPP17 || VITA3K_CPP14
#define VITA3K_CONSTEXPR_FUNC constexpr
#else
#define VITA3K_CONSTEXPR_FUNC
#endif
