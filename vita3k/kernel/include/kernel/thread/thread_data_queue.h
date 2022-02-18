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

#include <kernel/thread/thread_state.h>
#include <list>
#include <set>

template <typename T>
class ThreadDataQueueInteratorBase {
public:
    virtual ~ThreadDataQueueInteratorBase() = default;
    virtual bool operator==(const ThreadDataQueueInteratorBase &rhs) = 0;
    virtual ThreadDataQueueInteratorBase<T> &operator++() = 0;
    virtual ThreadDataQueueInteratorBase<T> *clone() const = 0;
    virtual T operator*() = 0;
};

template <typename T>
class ThreadDataQueueInterator {
public:
    ThreadDataQueueInterator(ThreadDataQueueInteratorBase<T> *base)
        : _base(base) {
    }

    ~ThreadDataQueueInterator() {
        delete _base;
    }

    ThreadDataQueueInterator(const ThreadDataQueueInterator<T> &o)
        : _base(o._base->clone()) {
    }

    ThreadDataQueueInterator<T> &operator=(const ThreadDataQueueInterator<T> &o) {
        delete _base;
        _base = o._base->clone();
        return *this;
    }

    bool operator==(const ThreadDataQueueInterator<T> &rhs) const {
        return *_base == *rhs._base;
    }

    bool operator!=(const ThreadDataQueueInterator<T> &rhs) const {
        return !(*this == rhs);
    }

    ThreadDataQueueInterator<T> &operator++() {
        ++(*_base);
        return *this;
    }

    ThreadDataQueueInterator<T> operator++(int) {
        auto last = _base->clone();
        _base->operator++();
        return ThreadDataQueueInterator<T>(last);
    }

    T operator*() {
        return **_base;
    }

    ThreadDataQueueInteratorBase<T> *base() const {
        return _base;
    }

private:
    ThreadDataQueueInteratorBase<T> *_base;
};

template <typename T>
class ThreadDataQueue {
public:
    virtual ThreadDataQueueInterator<T> begin() = 0;
    virtual ThreadDataQueueInterator<T> end() = 0;
    virtual void erase(const ThreadDataQueueInterator<T> &it) = 0;
    virtual void erase(const T &val) = 0;
    virtual void push(const T &val) = 0;
    virtual void pop() = 0;
    virtual bool empty() = 0;
    virtual size_t size() = 0;

    virtual ThreadDataQueueInterator<T> find(const T &val) = 0;
    virtual ThreadDataQueueInterator<T> find(const ThreadStatePtr &val) = 0;
    virtual ~ThreadDataQueue() = default;
};

template <typename T>
class FIFOThreadDataQueueInteratorBase : public ThreadDataQueueInteratorBase<T> {
public:
    FIFOThreadDataQueueInteratorBase(typename std::list<T>::iterator it)
        : it(it) {
    }

    ~FIFOThreadDataQueueInteratorBase() override = default;

    bool operator==(const ThreadDataQueueInteratorBase<T> &rhs) override {
        auto casted = static_cast<const FIFOThreadDataQueueInteratorBase<T> &>(rhs);
        return it == casted.it;
    }

    ThreadDataQueueInteratorBase<T> &operator++() override {
        ++it;
        return *this;
    }

    ThreadDataQueueInteratorBase<T> *clone() const override {
        return new FIFOThreadDataQueueInteratorBase<T>(it);
    }

    T operator*() override {
        return *it;
    }

    typename std::list<T>::iterator it;
};

template <typename T>
class FIFOThreadDataQueue : public ThreadDataQueue<T> {
public:
    FIFOThreadDataQueue() = default;

    ThreadDataQueueInterator<T> begin() override {
        return make_iterator(c.begin());
    }

    ThreadDataQueueInterator<T> end() override {
        return make_iterator(c.end());
    }

    void erase(const ThreadDataQueueInterator<T> &it) override {
        auto base = dynamic_cast<const FIFOThreadDataQueueInteratorBase<T> *>(it.base());
        c.erase(base->it);
    }

    void erase(const T &val) override {
        // TODO better search algo
        auto it = std::find(c.begin(), c.end(), val);
        c.erase(it);
    }

    void push(const T &val) override {
        c.push_back(val);
    }

    void pop() override {
        c.erase(c.begin());
    }

    size_t size() override {
        return c.size();
    }

    bool empty() override {
        return c.empty();
    }

    ThreadDataQueueInterator<T> find(const T &val) override {
        auto it = std::find(c.begin(), c.end(), val);
        return make_iterator(it);
    }

    ThreadDataQueueInterator<T> find(const ThreadStatePtr &val) override {
        auto it = std::find(c.begin(), c.end(), val);
        return make_iterator(it);
    }

private:
    ThreadDataQueueInterator<T> make_iterator(typename std::list<T>::iterator it) {
        auto base = new FIFOThreadDataQueueInteratorBase<T>(it);
        return ThreadDataQueueInterator<T>(base);
    }

    std::list<T> c;
};

template <typename T>
class PriorityThreadDataQueueInteratorBase : public ThreadDataQueueInteratorBase<T> {
public:
    PriorityThreadDataQueueInteratorBase(typename std::multiset<T, std::greater<T>>::iterator it)
        : it(it) {
    }

    ~PriorityThreadDataQueueInteratorBase() override = default;

    bool operator==(const ThreadDataQueueInteratorBase<T> &rhs) override {
        auto casted = static_cast<const PriorityThreadDataQueueInteratorBase<T> &>(rhs);
        return it == casted.it;
    }

    ThreadDataQueueInteratorBase<T> &operator++() override {
        ++it;
        return *this;
    }

    ThreadDataQueueInteratorBase<T> *clone() const override {
        return new PriorityThreadDataQueueInteratorBase<T>(it);
    }

    T operator*() override {
        return *it;
    }

    typename std::multiset<T>::iterator it;
};

template <typename T>
class PriorityThreadDataQueue : public ThreadDataQueue<T> {
public:
    PriorityThreadDataQueue() = default;

    ThreadDataQueueInterator<T> begin() override {
        return make_iterator(c.begin());
    }

    ThreadDataQueueInterator<T> end() override {
        return make_iterator(c.end());
    }

    void erase(const ThreadDataQueueInterator<T> &it) override {
        auto base = dynamic_cast<const PriorityThreadDataQueueInteratorBase<T> *>(it.base());
        c.erase(base->it);
    }

    void erase(const T &val) override {
        c.erase(val);
    }

    void push(const T &val) override {
        c.insert(val);
    }

    void pop() override {
        c.erase(c.begin());
    }

    size_t size() override {
        return c.size();
    }

    bool empty() override {
        return c.empty();
    }

    ThreadDataQueueInterator<T> find(const T &val) override {
        auto it = std::find(c.begin(), c.end(), val);
        return make_iterator(it);
    }

    ThreadDataQueueInterator<T> find(const ThreadStatePtr &val) override {
        auto it = std::find(c.begin(), c.end(), val);
        return make_iterator(it);
    }

private:
    ThreadDataQueueInterator<T> make_iterator(typename std::multiset<T, std::greater<T>>::iterator it) {
        auto base = new PriorityThreadDataQueueInteratorBase<T>(it);
        return ThreadDataQueueInterator<T>(base);
    }

    std::multiset<T> c;
};
