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

// Picks the link member a list uses in its Ts. The list reads it only inside its methods, so T can
// be incomplete where the list is declared, through a template like
// template <class T> struct WaitLink : MemberLink<&T::wait_link> {}.
template <auto Member>
struct MemberLink {
    static constexpr auto member = Member;
};

// A T's place in one IntrusiveList, a member of the T. Null when the T is in no list. linked() is
// read under the lock that guards the list it joins. A T in a list must stay where it is, so the
// link can't be copied or moved.
template <class T>
class IntrusiveLink {
public:
    IntrusiveLink() = default;
    IntrusiveLink(const IntrusiveLink &) = delete;
    IntrusiveLink &operator=(const IntrusiveLink &) = delete;
    ~IntrusiveLink() {
        assert(!linked());
    }

    bool linked() const {
        return next != nullptr;
    }

private:
    template <class U, class Selector>
    friend class IntrusiveList;

    T *prev = nullptr;
    T *next = nullptr;
};

// A circular doubly linked list of Ts through the link Selector picks, so adding and removing
// allocates nothing. The first T's prev is the last one, and a list's only T points at itself. It
// never owns or destroys its Ts, and the caller locks it.
template <class T, class Selector>
class IntrusiveList {
    static IntrusiveLink<T> &link(T &item) {
        static_assert(std::is_same_v<decltype(Selector::member), IntrusiveLink<T> T::*const>);
        return item.*Selector::member;
    }
    static const IntrusiveLink<T> &link(const T &item) {
        return item.*Selector::member;
    }

public:
    // Goes through the list in order, giving each T. It stays valid while its T is in the list.
    template <bool Const>
    class Iterator {
        using List = std::conditional_t<Const, const IntrusiveList, IntrusiveList>;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<Const, const T &, T &>;
        using pointer = std::conditional_t<Const, const T *, T *>;

        Iterator() = default;

        reference operator*() const {
            return *node;
        }
        pointer operator->() const {
            return node;
        }
        // The last T leads back to the first, which is the end
        Iterator &operator++() {
            node = link(*node).next;
            if (node == list->first)
                node = nullptr;
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

        Iterator(List *list, T *node)
            : list(list)
            , node(node) {}

        List *list = nullptr;
        // Null at the end
        T *node = nullptr;
    };

    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    IntrusiveList() = default;
    IntrusiveList(const IntrusiveList &) = delete;
    IntrusiveList &operator=(const IntrusiveList &) = delete;
    ~IntrusiveList() {
        assert(empty());
    }

    bool empty() const {
        return first == nullptr;
    }
    std::size_t size() const {
        return count;
    }
    iterator begin() {
        return iterator(this, first);
    }
    iterator end() {
        return iterator(this, nullptr);
    }
    const_iterator begin() const {
        return const_iterator(this, first);
    }
    const_iterator end() const {
        return const_iterator(this, nullptr);
    }

    T &front() {
        assert(!empty());
        return *first;
    }
    const T &front() const {
        assert(!empty());
        return *first;
    }

    // Adds item, which must be in no list through this link, before position, and returns where it is
    iterator insert(iterator position, T &item) {
        assert(position.list == this);
        IntrusiveLink<T> &node = link(item);
        assert(!node.linked());
        ++count;
        if (!first) {
            node.prev = &item;
            node.next = &item;
            first = &item;
            return iterator(this, &item);
        }
        // The end is behind the last T, which is in front of the first
        T &next = position.node ? *position.node : *first;
        node.prev = link(next).prev;
        node.next = &next;
        link(*node.prev).next = &item;
        link(next).prev = &item;
        if (position.node == first)
            first = &item;
        return iterator(this, &item);
    }
    void push_back(T &item) {
        insert(end(), item);
    }

    // Takes item out of the list if it is in it. item must be in this list or in none through this
    // link.
    void remove(T &item) {
        IntrusiveLink<T> &node = link(item);
        if (!node.linked())
            return;
        --count;
        if (node.next == &item) {
            first = nullptr;
        } else {
            link(*node.prev).next = node.next;
            link(*node.next).prev = node.prev;
            if (first == &item)
                first = node.next;
        }
        node.prev = nullptr;
        node.next = nullptr;
    }

private:
    T *first = nullptr;
    std::size_t count = 0;
};
