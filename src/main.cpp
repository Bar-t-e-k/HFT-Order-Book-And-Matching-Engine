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
            order_book.add_order(o.id, o.price, o.volume, o.side, o.type);

            processed.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void print_help() {
    std::cout << "\n--- AVAILABLE COMMANDS ---\n"
              << "buy <price> <volume> [limit/ioc/fok]  - Buy order (default limit)\n"
              << "sell <price> <volume> [limit/ioc/fok] - Sell order (default limit)\n"
              << "book                                  - Show best prices (Top of Book)\n"
              << "book full                             - Show full depth of market (DoM)\n"
              << "trades                                - Fetch and display trade history\n"
              << "bench                                 - Run performance test (1M orders)\n"
              << "exit                                  - Exit system\n"
              << "------------------------\n";
}

int main() {
    std::cout << "--- HFT Matching Engine Benchmark ---\n";

    ObjectPool<Order> order_pool(2'000'000);
    OrderBook order_book(order_pool, "NVDA");
    ThreadSafeQueue order_queue;
    TrafficGenerator traffic_gen(order_queue);

    std::atomic<uint64_t> processed_orders{0};
    uint64_t next_order_id = 1;

    std::cout << "[SYSTEM] Starting engine...\n";
    std::jthread engine_thread(engine_worker, std::ref(order_queue), std::ref(order_book), std::ref(processed_orders));

    print_help();

    std::string line;
    while (true) {
        std::cout << "\n(Stock market)> ";
        if (!std::getline(std::cin, line) || line.empty()) continue;

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "exit" || command == "quit") {
            std::cout << "[SYSTEM] Exiting...\n";
            break;
        }
        else if (command == "buy" || command == "sell") {
            uint32_t price = 0, volume = 0;
            std::string type_str = "limit";

            if (!(iss >> price >> volume)) {
                std::cout << "Error: Invalid format. Use: " << command << " <price> <volume> [type]\n";
                continue;
            }
            iss >> type_str;

            Side side = (command == "buy") ? Side::BUY : Side::SELL;
            OrderType type = OrderType::LIMIT;

            if (type_str == "ioc") type = OrderType::IOC;
            else if (type_str == "fok") type = OrderType::FOK;
            else if (type_str != "limit") {
                std::cout << "Error: Unknown order type. Using LIMIT by default.\n";
            }

            order_queue.push({next_order_id++, price, volume, side, type});
            std::cout << "-> Order sent to engine.\n";
        }
        else if (command == "book") {
            std::string sub_command;
            if (iss >> sub_command && sub_command == "full") {
                order_book.print_depth_of_market();
            } else {
                auto [best_bid, best_ask] = order_book.get_top_of_book();
                std::cout << "===== Top of Book =====\n";
                std::cout << "Best Sell (Ask): " << (best_ask < 100000 ? std::to_string(best_ask) : "NONE") << "\n";
                std::cout << "Best Buy (Bid): " << (best_bid > 0 ? std::to_string(best_bid) : "NONE") << "\n";
                std::cout << "=======================================\n";
            }
        }
        else if (command == "trades") {
            auto trades = order_book.retrieve_trades();
            if (trades.empty()) {
                std::cout << "No new trades.\n";
            } else {
                std::cout << "--- NEW TRADES ---\n";
                for (const auto& t : trades) {
                    std::cout << "[TRADE] " << t.symbol << " | Volume: " << t.volume << " @ Price: " << t.price
                              << " (Buyer ID: " << t.buy_order_id << " -> Seller ID: " << t.sell_order_id << ")\n";
                }
            }
        }
        else if (command == "bench") {
            std::cout << "[BENCHMARK] Starting traffic generator with 4 threads...\n";
            processed_orders = 0;
            auto wall_start = std::chrono::high_resolution_clock::now();

            traffic_gen.generate_traffic(4, 250'000);

            while (processed_orders.load(std::memory_order_relaxed) < 1'000'000) {
                std::this_thread::yield();
            }

            auto wall_end = std::chrono::high_resolution_clock::now();
            auto wall_duration = std::chrono::duration_cast<std::chrono::milliseconds>(wall_end - wall_start).count();

            std::cout << "Benchmark completed in " << wall_duration << " ms.\n";
        }
        else {
            std::cout << "Unknown command. Type 'buy', 'sell', 'book', 'trades', 'bench' or 'exit'.\n";
        }
    }

    return 0;
}