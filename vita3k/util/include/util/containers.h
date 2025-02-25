// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <boost/version.hpp>

#include <vector>

#if BOOST_VERSION >= 108200
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/unordered/unordered_node_map.hpp>
// use a boost unordered_flat_map as performance really matters for fields using this type
template <typename T, typename S>
using unordered_map_fast = boost::unordered::unordered_flat_map<T, S>;
template <typename T>
using unordered_set_fast = boost::unordered::unordered_flat_set<T>;
// this is in case we need pointer stability under rehashing
template <typename T, typename S>
using unordered_map_stable = boost::unordered::unordered_node_map<T, S>;
#else
// fallback in case someone is using an old boost version
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
template <typename T, typename S>
using unordered_map_fast = boost::unordered_map<T, S>;
template <typename T>
using unordered_set_fast = boost::unordered_set<T>;
template <typename T, typename S>
using unordered_map_stable = boost::unordered_map<T, S>;
#endif

namespace lru {

template <typename T>
struct Item {
    Item<T> *next;
    Item<T> *prev;
    T content;
};

// A container which supports getting the least recently used item and setting an item as the most recently used
template <typename T>
struct Queue {
    std::vector<Item<T>> items;
    Item<T> *head;

    void init(const size_t size) {
        // workaround in case T is not copy/move constructible
        items = std::vector<Item<T>>(size);

        // initialize the doubly linked list
        // make it so that items are first considered in the order of the array
        for (size_t i = 0; i < size; i++) {
            items[i].prev = &items[(i + 1) % size];
            items[i].next = &items[(i - 1 + size) % size];
        }
        head = &items[size - 1];
    }

    // get the least recently used element
    T *get_lru() const {
        // it is at the end of the list (so before the first one)
        return &head->prev->content;
    }

    // set an element as the most recently used
    void set_as_mru(T *ptr) {
        // get the item from a pointer to its content
        // removed the offsetof to avoid compilation warnings
        Item<T> *item = reinterpret_cast<Item<T> *>(reinterpret_cast<char *>(ptr) - 2 * sizeof(void *) /* offsetof(Item<T>, content)*/);
        if (item == head)
            return;

        // remove it from its position
        item->prev->next = item->next;
        item->next->prev = item->prev;

        // update the item itself
        item->prev = head->prev;
        item->next = head;

        // insert it as the head
        item->prev->next = item;
        item->next->prev = item;
        head = item;
    }

    // set an element as the least recently used (useful if it is removed)
    void set_as_lru(T *ptr) {
        // just set as mru and change the head
        set_as_mru(ptr);
        head = head->next;
    }
};
} // namespace lru
