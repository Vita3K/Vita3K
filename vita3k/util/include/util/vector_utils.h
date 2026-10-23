// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <algorithm>
#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vector_utils {

/**
 * \brief Merges and sorts two vectors. Also eliminates duplicates.
 * Optimal based on: http://stackoverflow.com/a/24477023
 * \param cur The current vector.
 * \param append The new vector to append. Always assume to be smaller than cur.
 * \return A vector of the same type as the inputs.
 */
template <typename T, typename A = std::allocator<T>, typename H = std::hash<T>, typename P = std::equal_to<T>>
std::vector<T, A> merge_vectors(const std::vector<T, A> &cur, const std::vector<T, A> &append) {
    std::vector<T, A> new_vector(cur.get_allocator());
    new_vector.reserve(cur.size() + append.size());
    new_vector.append_range(cur);
    new_vector.append_range(append);

    std::unordered_set<T, H, P, A> s;
    for (auto &&i : new_vector)
        s.insert(std::move(i));
    new_vector.assign_range(s);
    std::ranges::sort(new_vector, std::less<>{});
    return new_vector;
}

template <typename T, typename V>
size_t find_index(const T &v, const V &value) {
    auto it = std::ranges::find(v, value);
    if (it != v.end()) {
        // The value was found, return its index
        return std::distance(v.begin(), it);
    } else {
        // The value was not found, return -1
        return -1;
    }
}

template <typename T, typename V>
bool push_if_not_exists(T &v, const V &value) {
    if (!std::ranges::contains(v, value)) {
        v.push_back(value);
        return false;
    } else {
        return true;
    }
}

template <typename T, typename V>
bool erase_first(T &v, const V &value) {
    auto it = std::ranges::find(v, value);
    if (it != v.end()) {
        v.erase(it);
        return true;
    } else {
        return false;
    }
}

} // namespace vector_utils
