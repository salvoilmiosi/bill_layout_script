#ifndef SAFE_QUEUE
#define SAFE_QUEUE

#include <shared_mutex>
#include <queue>

template <typename T>
class safe_queue {
public:
    void enqueue(T t) {
        std::unique_lock lock(m);
        q.push(std::move(t));
    }

    T dequeue() {
        std::unique_lock lock(m);
        T val = q.front();
        q.pop();
        return val;
    }

    void clear() {
        std::unique_lock lock(m);
        q = {};
    }

    bool empty() {
        std::shared_lock lock(m);
        return q.empty();
    }

    size_t size() {
        std::shared_lock lock(m);
        return q.size();
    }

private:
    std::queue<T> q;
    std::shared_mutex m;
};
#endif