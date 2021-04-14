#ifndef SAFE_QUEUE
#define SAFE_QUEUE

#include <condition_variable>
#include <mutex>
#include <queue>

// A threadsafe-queue.
struct queue_timeout {};
template <typename T>
class safe_queue {
public:
    // Add an element to the queue.
    void enqueue(T t) {
        std::lock_guard<std::mutex> lock(m);
        q.push(std::move(t));
        c.notify_one();
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T dequeue() {
        using namespace std::chrono_literals;

        std::unique_lock<std::mutex> lock(m);
        while (q.empty()) {
            // release lock as long as the wait and reaquire it afterwards.
            c.wait_for(lock, 4s);
            throw queue_timeout{};
        }
        T val = q.front();
        q.pop();
        return val;
    }

    bool empty() {
        return q.empty();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m);
        q = {};
        c.notify_all();
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
    std::condition_variable c;
};
#endif