#pragma once

#include <atomic>
#include <map>
#include <mutex>

// Brought form rpcs3
class TypeInfo {
    friend class ObjectStore;
    // Global variable for each registered type
    template <typename T>
    struct registered {
        static const uint32_t index;
    };

    // Increment type counter
    static uint32_t add_type(uint32_t i) {
        static std::atomic<uint32_t> g_next{ 0 };

        return g_next.fetch_add(i);
    }

public:
    template <typename T>
    static inline uint32_t get_index() {
        return registered<T>::index;
    }

    static inline uint32_t get_count() {
        return add_type(0);
    }
};

template <typename T>
const uint32_t TypeInfo::registered<T>::index = TypeInfo::add_type(1);

class ObjectStore {
public:
    ObjectStore() {}

    ~ObjectStore() {}

    template <typename T>
    T *get() {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = objs.find(TypeInfo::registered<T>::index);
        assert(it != objs.end());
        return reinterpret_cast<T *>(it->second.get());
    }

    template <typename T, typename... Args>
    bool create(Args &&...args) {
        std::lock_guard<std::mutex> lock(mutex);
        auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
        objs.emplace(TypeInfo::registered<T>::index, ptr);
        return true;
    }
    template <typename T>
    void erase() {
        auto it = objs.find(TypeInfo::registered<T>::index);
        assert(it != objs.end());
        objs.erase(it);
    }

private:
    std::mutex mutex;
    std::map<uint32_t, std::shared_ptr<void>> objs;
};