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

#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>

// A T's place in one IntrusiveList, a member of the T. A T in a list must stay where it is, so the
// link can't be copied or moved, and it must leave the list before it is destroyed.
template <class T>
class IntrusiveLink {
public:
    IntrusiveLink() = default;
    IntrusiveLink(const IntrusiveLink &) = delete;
    IntrusiveLink &operator=(const IntrusiveLink &) = delete;
    ~IntrusiveLink() {
        assert(!linked());
    }

    // Whether the T is in a list, which tells an unlinked T from a list's only one
    bool linked() const {
        return list != nullptr;
    }

private:
    template <class U, IntrusiveLink<U> U::*Hook>
    friend class IntrusiveList;

    T *prev = nullptr;
    T *next = nullptr;
    const void *list = nullptr;
};

// A doubly linked list of Ts that links them through their IntrusiveLink member Hook, so adding
// and removing allocates nothing. It never owns or destroys its Ts, and the caller locks it.
template <class T, IntrusiveLink<T> T::*Hook>
class IntrusiveList {
    static_assert(Hook != nullptr);

public:
    // Goes through the list in order, giving each T
    template <bool Const>
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<Const, const T &, T &>;
        using pointer = std::conditional_t<Const, const T *, T *>;

        Iterator() = default;

        reference operator*() const {
            assert(node);
            return *node;
        }
        pointer operator->() const {
            assert(node);
            return node;
        }
        Iterator &operator++() {
            assert(node);
            node = link(*node).next;
            return *this;
        }
        Iterator operator++(int) {
            Iterator before = *this;
            ++*this;
            return before;
        }
        bool operator==(const Iterator &) const = default;

    private:
        friend class IntrusiveList;

        Iterator(const IntrusiveList *owner, T *node)
            : owner(owner)
            , node(node) {}

        const IntrusiveList *owner = nullptr;
        T *node = nullptr;
    };

    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    IntrusiveList() = default;
    // Its Ts point at it, so it doesn't move
    IntrusiveList(const IntrusiveList &) = delete;
    IntrusiveList &operator=(const IntrusiveList &) = delete;
    ~IntrusiveList() {
        assert(empty());
    }

    bool empty() const {
        return head == nullptr;
    }
    std::size_t size() const {
        return count;
    }
    iterator begin() {
        return { this, head };
    }
    iterator end() {
        return { this, nullptr };
    }
    const_iterator begin() const {
        return { this, head };
    }
    const_iterator end() const {
        return { this, nullptr };
    }

    T &front() {
        assert(head);
        return *head;
    }
    const T &front() const {
        assert(head);
        return *head;
    }

    // Adds value, which must be in no list, before position
    void insert(iterator position, T &value) {
        assert(position.owner == this);
        IntrusiveLink<T> &node = link(value);
        assert(!node.linked());
        T *const next = position.node;
        T *const prev = next ? link(*next).prev : tail;
        node.prev = prev;
        node.next = next;
        node.list = this;
        (prev ? link(*prev).next : head) = &value;
        (next ? link(*next).prev : tail) = &value;
        ++count;
    }
    void push_back(T &value) {
        insert(end(), value);
    }

    // Takes value out of the list, and returns whether it was in it. Removing a T that is in no list
    // does nothing, while one in another list asserts.
    bool remove(T &value) {
        IntrusiveLink<T> &node = link(value);
        if (!node.linked())
            return false;
        assert(node.list == this);
        (node.prev ? link(*node.prev).next : head) = node.next;
        (node.next ? link(*node.next).prev : tail) = node.prev;
        node.prev = nullptr;
        node.next = nullptr;
        node.list = nullptr;
        --count;
        return true;
    }

private:
    static IntrusiveLink<T> &link(T &value) {
        return value.*Hook;
    }
    static const IntrusiveLink<T> &link(const T &value) {
        return value.*Hook;
    }

    T *head = nullptr;
    T *tail = nullptr;
    std::size_t count = 0;
};
