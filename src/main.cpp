#include <iostream>
#include <thread>
#include <chrono>
#include "Order.hpp"
#include "ObjectPool.hpp"
#include "OrderBook.hpp"
#include "ThreadSafeQueue.hpp"

void engine_worker(std::stop_token stoken, ThreadSafeQueue& queue, OrderBook& order_book) {
    std::cout << "[ENGINE] Engine thread started.\n";

    while (!stoken.stop_requested()) {
        auto order_opt = queue.pop(stoken);

        if (order_opt.has_value()) {
            auto& o = order_opt.value();
            order_book.add_order(o.id, o.price, o.volume, o.side);
        }
    }
    std::cout << "[ENGINE] Engine thread finished (Graceful Shutdown).\n";
}

int main() {
    std::cout << "--- HFT Matching Engine Initialization ---\n";

    ObjectPool<Order> order_pool(1'000'000);

    OrderBook order_book(order_pool, "AAPL");

    ThreadSafeQueue order_queue;

    std::jthread engine_thread(engine_worker, std::ref(order_queue), std::ref(order_book));

    std::cout << "Generating orders...\n";

    order_book.add_order(1ULL, 1000U, 100U, Side::SELL);
    order_book.add_order(2ULL, 990U, 50U, Side::SELL);
    order_book.add_order(3ULL, 1000U, 70U, Side::BUY);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto [best_bid, best_ask] = order_book.get_top_of_book();
    std::cout << "[BOT] Best bid: " << best_bid << ", Best ask: " << best_ask << "\n";

    auto trades = order_book.retrieve_trades();
    std::cout << "\n--- TRADE HISTORY REGISTERED BY THE ENGINE ---\n";
    for (const auto& trade : trades) {
        std::cout << "[TRADE] Symbol: " << trade.symbol
                  << " | Buy Order ID: " << trade.buy_order_id
                  << " -> Sell Order ID: " << trade.sell_order_id
                  << " | Volume: " << trade.volume
                  << " @ Price: " << trade.price << "\n";
    }
    std::cout << "-----------------------------------------------------\n\n";

    return 0;
}
