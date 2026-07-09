#pragma once
#include <cstdint>
#include <concepts>

template <typename T>
concept IntegralNumeric = std::integral<T>;

enum class Side : uint8_t {
    BUY,
    SELL
};

struct alignas(64) Order {
    uint64_t id;
    uint32_t price;
    uint32_t volume;
    Side side;

    void reset(uint64_t new_id, uint32_t new_price, uint32_t new_volume, Side new_side) {
        id = new_id;
        price = new_price;
        volume = new_volume;
        side = new_side;
    }
};