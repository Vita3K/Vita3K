#pragma once

#include <cstdint>
#include <set>

// Don't use unordered_set since searching is usually slower for < 100 elements
using NIDSet = std::set<std::uint32_t>;
