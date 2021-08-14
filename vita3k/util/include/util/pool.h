// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <vector>

template <typename T>
class PoolItem;

// Pool class for public static resources
template <typename T>
class Pool {
public:
    typedef std::unique_ptr<T, std::function<void(T *)>> Ptr;
    Pool() {
    }

    void add(Ptr item) {
        free_ids.insert(items.size());
        items.push_back(std::move(item));
    }

    int available() {
        std::unique_lock<std::mutex> lock(mutex);
        return free_ids.size();
    }

    // borrow returns PoolItem instance wrapping one of available resources in the pool
    // if there's no available resource, the thread will wait until resources are returned to the pool
    // if PoolItem instance is destroyed, the corresponding resource is considerd to be returned to the pool
    PoolItem<T> borrow();

private:
    void release(int id) {
        std::unique_lock<std::mutex> lock(mutex);
        free_ids.insert(id);
        cond.notify_one();
    }

    Pool(const Pool &);
    const Pool &operator=(const Pool &);

    std::mutex mutex;
    std::condition_variable cond;
    std::vector<Ptr> items;
    std::set<int> free_ids;

    friend class PoolItem<T>;
};

template <typename T>
class PoolItem {
public:
    PoolItem(Pool<T> *parent, T *item, int id)
        : parent(parent)
        , item(item)
        , id(id) {
    }

    T *get() {
        return item;
    }

    ~PoolItem() {
        parent->release(id);
    }

private:
    PoolItem(const PoolItem &);
    const PoolItem &operator=(const PoolItem &);

    Pool<T> *parent;
    int id;
    T *item;
};

template <typename T>
PoolItem<T> Pool<T>::borrow() {
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [&] { return !free_ids.empty(); });
    auto it = free_ids.begin();
    int id = *it;
    free_ids.erase(it);
    return PoolItem<T>(this, items[id].get(), id);
}
