#pragma once

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)

#define STRINGIZE_DETAIL(x) #x ""
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define STR_CASE(...) \
    case __VA_ARGS__: \
        return #__VA_ARGS__
