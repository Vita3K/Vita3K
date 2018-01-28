#pragma once

#include <map>
#include <memory>

template <typename T, typename Key>
std::shared_ptr<T> find(Key key, const std::map<Key, std::shared_ptr<T>> &map) {
    const typename std::map<Key, std::shared_ptr<T>>::const_iterator it = map.find(key);
    if (it == map.end()) {
        return std::shared_ptr<T>();
    }

    return it->second;
}
