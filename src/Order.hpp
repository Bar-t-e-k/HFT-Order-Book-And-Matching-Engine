#pragma once
#include <cstdint>
#include <concepts>

template <typename T>
concept ValidNumericType = std::unsigned_integral<T> && (sizeof(T) >= 4);

enum class Side : uint8_t {
    BUY,
    SELL
};

struct alignas(64) TradeReport {
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    uint32_t price;
    uint32_t volume;
    std::string_view symbol;
};

struct alignas(64) Order {
    uint64_t id;
    uint32_t price;
    uint32_t volume;
    Side side;
    std::string_view symbol;

    Order* next{nullptr};

    void reset(uint64_t new_id, uint32_t new_price, uint32_t new_volume, Side new_side, std::string_view new_symbol) {
        id = new_id;
        price = new_price;
        volume = new_volume;
        side = new_side;
        symbol = new_symbol;
        next = nullptr;
    }
};