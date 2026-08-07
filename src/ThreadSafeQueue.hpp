#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <stop_token>

struct IncomingOrder {
    uint64_t id;
    uint32_t price;
    uint32_t volume;
    Side side;
    OrderType type;
};

class ThreadSafeQueue {
    std::queue<IncomingOrder> queue;
    std::mutex mtx;
    std::condition_variable_any cv; 

public:
    void push(const IncomingOrder& order) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(order);
        }
        cv.notify_one();
    }

    std::optional<IncomingOrder> pop(std::stop_token stoken) {
        std::unique_lock<std::mutex> lock(mtx);

        cv.wait(lock, stoken, [this]() { return !queue.empty(); });

        if (stoken.stop_requested()) {
            return std::nullopt;
        }

        IncomingOrder order = queue.front();
        queue.pop();
        return order;
    }
};