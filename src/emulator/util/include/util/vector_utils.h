#pragma once

#include <unordered_set>
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
    std::vector<T, A> new_vector = cur;
    new_vector.insert(new_vector.end(), append.begin(), append.end());

    std::unordered_set<T, H, P, A> s;
    for (const auto &i : new_vector)
        s.insert(i);
    new_vector.assign(s.begin(), s.end());
    std::sort(new_vector.begin(), new_vector.end());
    return new_vector;
}

} // namespace vector_utils
