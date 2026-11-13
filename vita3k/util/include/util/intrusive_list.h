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
#include <memory>
#include <type_traits>

// A T's place in one IntrusiveList, a base class of the T. A node in no list points at itself.
// A T in a list must stay where it is, so the link can't be copied or moved.
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
        return next != this;
    }

private:
    template <class U>
    friend class IntrusiveList;

    // Takes the node out of its list and points it back at itself
    void unlink() {
        prev->next = next;
        next->prev = prev;
        prev = this;
        next = this;
    }

    IntrusiveLink *prev = this;
    IntrusiveLink *next = this;
};

// A circular doubly linked list of Ts around a sentinel head, so adding and removing allocates
// nothing. It never owns or destroys its Ts, and the caller locks it.
template <class T>
class IntrusiveList {
public:
    // Goes through the list in order, giving each T
    template <bool Const>
    class Iterator {
        using Link = std::conditional_t<Const, const IntrusiveLink<T>, IntrusiveLink<T>>;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<Const, const T &, T &>;
        using pointer = std::conditional_t<Const, const T *, T *>;

        Iterator() = default;

        reference operator*() const {
            return static_cast<reference>(*node);
        }
        pointer operator->() const {
            return &**this;
        }
        Iterator &operator++() {
            node = node->next;
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

        explicit Iterator(Link *node)
            : node(node) {}

        Link *node = nullptr;
    };

    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    bool empty() const {
        return !head.linked();
    }
    std::size_t size() const {
        return count;
    }
    iterator begin() {
        return iterator(head.next);
    }
    iterator end() {
        return iterator(&head);
    }
    const_iterator begin() const {
        return const_iterator(head.next);
    }
    const_iterator end() const {
        return const_iterator(&head);
    }

    T &front() {
        assert(!empty());
        return *begin();
    }
    const T &front() const {
        assert(!empty());
        return *begin();
    }

    // Adds value, which must be in no list, before position
    void insert(iterator position, T &value) {
        IntrusiveLink<T> &node = value;
        assert(!node.linked());
        IntrusiveLink<T> *const next = position.node;
        node.prev = next->prev;
        node.next = next;
        next->prev->next = &node;
        next->prev = &node;
        ++count;
    }
    void push_back(T &value) {
        insert(end(), value);
    }

    // Takes value out of the list if it is in it. value must not be in another list.
    void remove(T &value) {
        IntrusiveLink<T> &node = value;
        if (!node.linked())
            return;
        node.unlink();
        --count;
    }

private:
    IntrusiveLink<T> head;
    std::size_t count = 0;
};

// An intrusive list that owns the objects on it. The caller locks it.
template <class T>
class UniqueList {
public:
    // Destroys every object still on the list
    ~UniqueList() {
        while (!list.empty())
            remove(list.front());
    }

    // Whether no object is on the list
    bool empty() const {
        return list.empty();
    }

    // The objects in list order
    auto begin() {
        return list.begin();
    }

    // The end of the objects in list order
    auto end() {
        return list.end();
    }

    // Links object before position and takes it over
    void insert(typename IntrusiveList<T>::iterator position, std::unique_ptr<T> object) {
        assert(object);
        list.insert(position, *object.release());
    }

    // Takes object, which must be on the list, off it and gives it back to the caller
    std::unique_ptr<T> remove(T &object) {
        assert(object.linked());
        list.remove(object);
        return std::unique_ptr<T>(&object);
    }

private:
    IntrusiveList<T> list;
};
