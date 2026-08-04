#pragma once
#include "ThreadSafeQueue.hpp"
#include <vector>
#include <thread>
#include <random>

#include "Order.hpp"

class TrafficGenerator {
    ThreadSafeQueue& queue;

public:
    explicit TrafficGenerator(ThreadSafeQueue& q) : queue(q) {}

    void generate_traffic(int num_threads, int orders_per_thread) {
        std::vector<std::thread> workers;
        workers.reserve(num_threads);

        for (int i = 0; i < num_threads; ++i) {
            workers.emplace_back([this, orders_per_thread, i]() {
                std::mt19937 rng(std::random_device{}() ^ i);
                std::uniform_int_distribution<uint32_t> price_dist(950, 1050);
                std::uniform_int_distribution<uint32_t> vol_dist(10, 100);
                std::uniform_int_distribution<int> side_dist(0, 1);

                for (int j = 0; j < orders_per_thread; ++j) {
                    IncomingOrder order{
                        .id = static_cast<uint64_t>(i * 1000000 + j),
                        .price = price_dist(rng),
                        .volume = vol_dist(rng),
                        .side = side_dist(rng) == 0 ? Side::BUY : Side::SELL
                    };

                    queue.push(order);
                }
            });
        }

        for (auto& w : workers) {
            w.join();
        }
    }
};
