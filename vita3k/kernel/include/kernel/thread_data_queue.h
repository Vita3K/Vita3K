// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <kernel/thread/thread_state.h>
#include <list>
#include <set>

template <typename T>
class ThreadDataQueueInteratorBase {
public:
    virtual ~ThreadDataQueueInteratorBase(){};
    virtual bool operator==(const ThreadDataQueueInteratorBase &rhs) = 0;
    virtual ThreadDataQueueInteratorBase &operator++() = 0;
    virtual ThreadDataQueueInteratorBase *clone() const = 0;
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

    ThreadDataQueueInterator(const ThreadDataQueueInterator &o)
        : _base(o._base->clone()) {
    }

    ThreadDataQueueInterator &operator=(const ThreadDataQueueInterator &o) {
        delete _base;
        _base = o._base->clone();
        return *this;
    }

    bool operator==(const ThreadDataQueueInterator &rhs) const {
        return *_base == *rhs._base;
    }

    bool operator!=(const ThreadDataQueueInterator &rhs) const {
        return !(*this == rhs);
    }

    ThreadDataQueueInterator &operator++() {
        ++(*_base);
        return *this;
    }
    ThreadDataQueueInterator operator++(int) {
        auto last = _base->clone();
        _base->operator++();
        return ThreadDataQueueInterator(last);
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
};

template <typename T>
class FIFOThreadDataQueueInteratorBase : public ThreadDataQueueInteratorBase<T> {
public:
    FIFOThreadDataQueueInteratorBase(typename std::list<T>::iterator it)
        : it(it) {
    }

    virtual ~FIFOThreadDataQueueInteratorBase() {}

    virtual bool operator==(const ThreadDataQueueInteratorBase<T> &rhs) {
        auto casted = static_cast<const FIFOThreadDataQueueInteratorBase &>(rhs);
        return it == casted.it;
    }
    virtual ThreadDataQueueInteratorBase &operator++() {
        ++it;
        return *this;
    }
    virtual ThreadDataQueueInteratorBase *clone() const {
        return new FIFOThreadDataQueueInteratorBase(it);
    }
    virtual T operator*() {
        return *it;
    }

    typename std::list<T>::iterator it;
};

template <typename T>
class FIFOThreadDataQueue : public ThreadDataQueue<T> {
public:
    FIFOThreadDataQueue() {
    }

    virtual ThreadDataQueueInterator<T> begin() {
        return make_iterator(c.begin());
    }

    virtual ThreadDataQueueInterator<T> end() {
        return make_iterator(c.end());
    }

    virtual void erase(const ThreadDataQueueInterator<T> &it) {
        auto base = dynamic_cast<const FIFOThreadDataQueueInteratorBase<T> *>(it.base());
        c.erase(base->it);
    }

    virtual void erase(const T &val) {
        // TODO better search algo
        auto it = std::find(c.begin(), c.end(), val);
        c.erase(it);
    }

    virtual void push(const T &val) {
        c.push_back(val);
    }

    virtual void pop() {
        c.erase(c.begin());
    }

    virtual size_t size() {
        return c.size();
    }

    virtual bool empty() {
        return c.empty();
    }

    virtual ThreadDataQueueInterator<T> find(const T &val) {
        auto it = std::find(c.begin(), c.end(), val);
        return make_iterator(it);
    }
    virtual ThreadDataQueueInterator<T> find(const ThreadStatePtr &val) {
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

    virtual ~PriorityThreadDataQueueInteratorBase() {}

    virtual bool operator==(const ThreadDataQueueInteratorBase<T> &rhs) {
        auto casted = static_cast<const PriorityThreadDataQueueInteratorBase &>(rhs);
        return it == casted.it;
    }
    virtual ThreadDataQueueInteratorBase &operator++() {
        ++it;
        return *this;
    }
    virtual ThreadDataQueueInteratorBase *clone() const {
        return new PriorityThreadDataQueueInteratorBase(it);
    }
    virtual T operator*() {
        return *it;
    }

    typename std::multiset<T, std::greater<T>>::iterator it;
};

template <typename T>
class PriorityThreadDataQueue : public ThreadDataQueue<T> {
public:
    PriorityThreadDataQueue() {}

    virtual ThreadDataQueueInterator<T> begin() {
        return make_iterator(c.begin());
    }

    virtual ThreadDataQueueInterator<T> end() {
        return make_iterator(c.end());
    }

    virtual void erase(const ThreadDataQueueInterator<T> &it) {
        auto base = dynamic_cast<const PriorityThreadDataQueueInteratorBase<T> *>(it.base());
        c.erase(base->it);
    }

    virtual void erase(const T &val) {
        c.erase(val);
    }

    virtual void push(const T &val) {
        c.insert(val);
    }

    virtual void pop() {
        c.erase(c.begin());
    }

    virtual size_t size() {
        return c.size();
    }

    virtual bool empty() {
        return c.empty();
    }

    virtual ThreadDataQueueInterator<T> find(const T &val) {
        auto it = std::find(c.begin(), c.end(), val);
        return make_iterator(it);
    }
    virtual ThreadDataQueueInterator<T> find(const ThreadStatePtr &val) {
        auto it = std::find(c.begin(), c.end(), val);
        return make_iterator(it);
    }

private:
    ThreadDataQueueInterator<T> make_iterator(typename std::multiset<T, std::greater<T>>::iterator it) {
        auto base = new PriorityThreadDataQueueInteratorBase<T>(it);
        return ThreadDataQueueInterator<T>(base);
    }

    std::multiset<T, std::greater<T>> c;
};
