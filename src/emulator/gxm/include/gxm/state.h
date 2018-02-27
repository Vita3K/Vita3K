#pragma once

#include <mem/ptr.h>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace emu {
    typedef void SceGxmDisplayQueueCallback(Ptr<const void> callbackData);

    struct SceGxmInitializeParams {
        uint32_t flags = 0;
        uint32_t displayQueueMaxPendingCount = 0;
        Ptr<SceGxmDisplayQueueCallback> displayQueueCallback;
        uint32_t displayQueueCallbackDataSize = 0;
        uint32_t parameterBufferSize = 0;
    };
}

template <typename T>
class Queue
{
public:

    uint32_t displayQueueMaxPendingCount_;

    
    T pop()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while(queue_.empty())
        {
            condempty_.wait(mlock);
        }
        auto item = queue_.front();
        queue_.pop();
        mlock.unlock();
        cond_.notify_one();
        return item;
    }
    
    void pop(T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while(queue_.empty())
        {
            condempty_.wait(mlock);
        }
        item = queue_.front();
        queue_.pop();
        mlock.unlock();
        cond_.notify_one();
    }
    
    void push(const T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.size()==displayQueueMaxPendingCount_)
        {
            cond_.wait(mlock);
        }
        queue_.push(item);
        mlock.unlock();
        condempty_.notify_one();
    }
    
    void push(T&& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.size()==displayQueueMaxPendingCount_)
        {
            cond_.wait(mlock);
        }
        queue_.push(std::move(item));
        mlock.unlock();
        condempty_.notify_one();
    }
    
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::condition_variable condempty_;
};

struct DisplayCallback {
    Address pc;
    Address data;
};

struct GxmState {
    emu::SceGxmInitializeParams params;
    bool isInScene = false;
    Queue<DisplayCallback> display_queue;
};
