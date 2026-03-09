#ifndef ARN_UTIL_BLOCKING_QUEUE_HPP
#define ARN_UTIL_BLOCKING_QUEUE_HPP
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>
#include "graceful_stop.hpp"

template<class T>
class BlockingQueue {
public:
    void push(T v) {
        {
            std::lock_guard lk(m);
            q.push(std::move(v));
        }
        cv.notify_one();
    }

    void notify_one() {
        cv.notify_one();
    }

    std::optional<T> pop_wait() {
        std::unique_lock lk(m);
        cv.wait(lk, [&]{
            return !q.empty() || is_stopped();
        });
        if (is_stopped())
            return {};
        if (q.empty())
            return {};
        T out = std::move(q.front());
        q.pop();
        return out;
    }

private:
    std::mutex m;
    std::condition_variable cv;
    std::queue<T> q;
};

#endif //ARN_UTIL_BLOCKING_QUEUE_HPP
