#ifndef queue_h
#define queue_h

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <memory>

template <typename T>
class Queue {
public:
    unsigned int displayQueueMaxPendingCount_;

    std::unique_ptr<T> pop() {
        T item{ T() };
        {
            std::unique_lock<std::mutex> mlock(mutex_);
            while (!aborted && queue_.empty()) {
                condempty_.wait(mlock);
            }
            if (aborted) {
                // releasing the mutex on Windows results in it not 
                // being unlocked by the destructor
#ifndef WIN32
                mlock.release();
#endif
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
            while (!aborted && queue_.size() == displayQueueMaxPendingCount_) {
                cond_.wait(mlock);
            }
            if (aborted) {
                // releasing the mutex on Windows results in it not 
                // being unlocked by the destructor
#ifndef WIN32
                mlock.release();
#endif
                return;
            }
            queue_.push(item);
        }
        condempty_.notify_one();
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
