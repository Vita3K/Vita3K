#pragma once

#include <functional>

template <typename T>
class Resource {
public:
    typedef std::function<void(T)> Deleter;

    Resource(T t, Deleter deleter)
        : t(t)
        , deleter(deleter) {
    }

    ~Resource() {
        if (deleter) {
            deleter(t);
        }
    }

    T get() const {
        return t;
    }

private:
    Resource(const Resource &);
    const Resource &operator=(const Resource &);

    T t;
    Deleter deleter;
};
