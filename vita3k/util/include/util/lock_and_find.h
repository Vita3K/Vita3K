#pragma once

#include "find.h"

#include <mutex>

template <typename T, typename Key>
std::shared_ptr<T> lock_and_find(Key key, const std::map<Key, std::shared_ptr<T>> &map, std::mutex &mutex) {
    const std::lock_guard<std::mutex> lock(mutex);
    return util::find(key, map);
}
