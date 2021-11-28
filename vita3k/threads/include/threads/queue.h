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

#ifndef queue_h
#define queue_h

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

template <typename T>
class Queue {
public:
    unsigned int maxPendingCount_;

    std::unique_ptr<T> pop(const int ms = 0) {
        T item{ T() };
        {
            std::unique_lock<std::mutex> mlock(mutex_);
            if (ms == 0) {
                while (!aborted && queue_.empty()) {
                    condempty_.wait(mlock);
                }
            } else {
                if (queue_.empty()) {
                    condempty_.wait_for(mlock, std::chrono::microseconds(ms));
                }
            }
            if (aborted || queue_.empty()) {
                return {};
            }

            item = queue_.front();
            queue_.pop();
        }
        cond_.notify_one();
        return std::make_unique<T>(item);
    }

    void push(const T &item) {
        {
            std::unique_lock<std::mutex> mlock(mutex_);
            while (!aborted && queue_.size() == maxPendingCount_) {
                cond_.wait(mlock);
            }
            if (aborted) {
                return;
            }
            queue_.push(item);
        }
        condempty_.notify_one();
    }

    size_t size() {
        return queue_.size();
    }

    void abort() {
        aborted = true;
        condempty_.notify_all();
        cond_.notify_all();
    }

    void reset() {
        queue_.clear();
        aborted = false;
    }

    Queue() = default;
    Queue(const Queue &) = delete; // disable copying
    Queue &operator=(const Queue &) = delete; // disable assignment

private:
    std::condition_variable cond_;
    std::condition_variable condempty_;
    std::queue<T> queue_;
    std::mutex mutex_;
    std::atomic<bool> aborted{ false };
};

#endif /* queue_h */
