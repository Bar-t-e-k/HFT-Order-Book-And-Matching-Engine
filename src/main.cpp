#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "ObjectPool.hpp"
#include "OrderBook.hpp"
#include "ThreadSafeQueue.hpp"
#include "TrafficGenerator.hpp"

void engine_worker(std::stop_token stoken, ThreadSafeQueue& queue, OrderBook& order_book, std::atomic<uint64_t>& processed) {
    while (!stoken.stop_requested()) {
        auto order_opt = queue.pop(stoken);

        if (order_opt.has_value()) {
            auto& o = order_opt.value();
            order_book.add_order(o.id, o.price, o.volume, o.side);

            processed.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

int main() {
    std::cout << "--- HFT Matching Engine Benchmark ---\n";

    ObjectPool<Order> order_pool(2'000'000);
    OrderBook order_book(order_pool, "NVDA");
    ThreadSafeQueue order_queue;
    TrafficGenerator traffic_gen(order_queue);

    std::atomic<uint64_t> processed_orders{0};

    const int NUM_THREADS = 4;
    const int ORDERS_PER_THREAD = 250'000;
    const uint64_t TOTAL_ORDERS = NUM_THREADS * ORDERS_PER_THREAD;

    std::cout << "[SYSTEM] Starting engine...\n";
    std::jthread engine_thread(engine_worker, std::ref(order_queue), std::ref(order_book), std::ref(processed_orders));

    std::cout << "[SYSTEM] Generating " << TOTAL_ORDERS << " orders from " << NUM_THREADS << " threads...\n";

    // ---------------------------------------------------------
    // B E N C H M A R K   S T A R T
    // ---------------------------------------------------------
    auto wall_start = std::chrono::high_resolution_clock::now();
    clock_t cpu_start = clock();

    traffic_gen.generate_traffic(NUM_THREADS, ORDERS_PER_THREAD);

    while (processed_orders.load(std::memory_order_relaxed) < TOTAL_ORDERS) {
        std::this_thread::yield();
    }

    clock_t cpu_end = clock();
    auto wall_end = std::chrono::high_resolution_clock::now();
    // ---------------------------------------------------------
    // B E N C H M A R K   E N D
    // ---------------------------------------------------------

    auto wall_duration = std::chrono::duration_cast<std::chrono::milliseconds>(wall_end - wall_start).count();
    double cpu_duration = 1000.0 * (cpu_end - cpu_start) / CLOCKS_PER_SEC;

    auto nanoseconds_per_order = (wall_duration * 1'000'000) / TOTAL_ORDERS;
    auto throughput = (TOTAL_ORDERS * 1000) / (wall_duration > 0 ? wall_duration : 1);

    auto trades = order_book.retrieve_trades();

    std::cout << "\n================ RESULTS BENCHMARK ================\n";
    std::cout << "Processed orders:  " << processed_orders.load() << "\n";
    std::cout << "Executed trades: " << trades.size() << "\n";
    std::cout << "---------------------------------------------------\n";
    std::cout << "Wall-clock time: " << wall_duration << " ms\n";
    std::cout << "CPU time:     " << cpu_duration << " ms\n";
    std::cout << "Throughput:                 " << throughput << " orders / second\n";
    std::cout << "Average latency:  " << nanoseconds_per_order << " nanoseconds / order\n";
    std::cout << "===================================================\n";

    return 0;
}