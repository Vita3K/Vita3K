#pragma once

#include <map>
#include <memory>

namespace util {

template <typename T, typename Key>
std::shared_ptr<T> find(Key key, const std::map<Key, std::shared_ptr<T>> &map) {
    const auto it = map.find(key);
    if (it == map.end()) {
        return std::shared_ptr<T>();
    }

    return it->second;
}

} // namespace util
