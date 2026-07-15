#include <iostream>
#include "Order.hpp"
#include "ObjectPool.hpp"
#include "OrderBook.hpp"

int main() {
    std::cout << "--- HFT Matching Engine Initialization ---\n";

    ObjectPool<Order> order_pool(1'000'000);

    OrderBook order_book(order_pool, "AAPL");

    std::cout << "Wrzucam zlecenia...\n";

    order_book.add_order(1ULL, 1000U, 100U, Side::SELL);
    order_book.add_order(2ULL, 990U, 50U, Side::SELL);
    order_book.add_order(3ULL, 1000U, 70U, Side::BUY);

    auto trades = order_book.retrieve_trades();

    std::cout << "\n--- HISTORIA TRANSAKCJI (Przetwarzana poza krytyczną pętlą) ---\n";
    for (const auto& trade : trades) {
        std::cout << "[TRADE] Symbol: " << trade.symbol
                  << " | Kupujacy ID: " << trade.buy_order_id
                  << " -> Sprzedajacy ID: " << trade.sell_order_id
                  << " | Ilosc: " << trade.volume
                  << " @ Cena: " << trade.price << "\n";
    }

    std::cout << "\nDostepne zlecenia w puli po handlu: " << order_pool.available() << "\n";

    return 0;
}
