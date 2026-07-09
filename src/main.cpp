#include <iostream>
#include "Order.hpp"
#include "ObjectPool.hpp"

int main() {
    std::cout << "--- HFT Matching Engine Initialization ---\n";

    std::cout << "Rozmiar struktury Order: " << sizeof(Order) << " bajtow\n";
    if (sizeof(Order) != 64) {
        std::cerr << "OSTRZEZENIE: Struktura Order nie jest wyrownana do 64 bajtow!\n";
    }

    try {
        std::cout << "Alokacja puli na 1 000 000 zlecen...\n";
        ObjectPool<Order> order_pool(1'000'000);

        std::cout << "Dostepne obiekty po starcie: " << order_pool.available() << "\n";

        Order* new_order = order_pool.acquire();
        new_order->reset(1, 1050, 100, Side::BUY); // ID: 1, Cena: 10.50, Ilosc: 100

        std::cout << "Pobrano zlecenie z puli. Zostalo wolnych: " << order_pool.available() << "\n";

        order_pool.release(new_order);
        std::cout << "Zwrocono zlecenie do puli. Zostalo wolnych: " << order_pool.available() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Blad krytyczny: " << e.what() << "\n";
    }

    return 0;
}
