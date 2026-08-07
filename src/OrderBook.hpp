#pragma once
#include "Order.hpp"
#include "ObjectPool.hpp"
#include <vector>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <shared_mutex>

class OrderBook {
    struct PriceLevel {
        Order* head = nullptr;
        Order* tail = nullptr;

        void push_back(Order* order) {
            order->next = nullptr;
            if (!head) {
                head = tail = order;
            } else {
                tail->next = order;
                tail = order;
            }
        }

        Order* pop_front() {
            if (!head) return nullptr;
            Order* order = head;
            head = head->next;
            if (!head) tail = nullptr;
            return order;
        }
        
        [[nodiscard]] bool empty() const { return head == nullptr; }
    };

    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;

    uint32_t best_bid = 0;
    uint32_t best_ask;
    size_t max_price_limit;

    std::vector<TradeReport> trade_history;
    ObjectPool<Order>& order_pool;
    std::string_view book_symbol;
    mutable std::shared_mutex rw_mutex;

public:
    explicit OrderBook(ObjectPool<Order>& pool, std::string_view symbol, size_t max_price = 100000)
        : bids(max_price), asks(max_price), best_ask(max_price - 1), max_price_limit(max_price), order_pool(pool), book_symbol(symbol) {
        trade_history.reserve(10000);
    }

    std::pair<uint32_t, uint32_t> get_top_of_book() const {
        std::shared_lock<std::shared_mutex> lock(rw_mutex);
        return {best_bid, best_ask};
    }

    template <ValidNumericType T_ID, ValidNumericType T_Price, ValidNumericType T_Vol>
    void add_order(T_ID id, T_Price price, T_Vol volume, Side side, OrderType type = OrderType::LIMIT) {
        std::unique_lock<std::shared_mutex> lock(rw_mutex);

        if (type == OrderType::FOK) {
            if (!can_fully_fill(price, volume, side)) {
                return;
            }
        }

        uint32_t remaining_volume = volume;

        if (side == Side::BUY) {
            while (remaining_volume > 0 && best_ask <= price) {
                PriceLevel& ask_level = asks[best_ask];
                while (!ask_level.empty() && remaining_volume > 0) {
                    Order* sell_order = ask_level.head;
                    uint32_t trade_vol = std::min(remaining_volume, sell_order->volume);

                    remaining_volume -= trade_vol;
                    sell_order->volume -= trade_vol;

                    trade_history.emplace_back(TradeReport{
                        .buy_order_id = id,
                        .sell_order_id = sell_order->id,
                        .price = best_ask,
                        .volume = trade_vol,
                        .symbol = book_symbol
                    });

                    if (sell_order->volume == 0) {
                        ask_level.pop_front();
                        order_pool.release(sell_order);
                    }
                }

                if (ask_level.empty()) {
                    best_ask++;
                    while (best_ask < max_price_limit && asks[best_ask].empty()) {
                        best_ask++;
                    }
                }
            }
        } else {
            while (remaining_volume > 0 && best_bid >= price && best_bid > 0) {
                PriceLevel& bid_level = bids[best_bid];
                while (!bid_level.empty() && remaining_volume > 0) {
                    Order* buy_order = bid_level.head;
                    uint32_t trade_vol = std::min(remaining_volume, buy_order->volume);

                    remaining_volume -= trade_vol;
                    buy_order->volume -= trade_vol;

                    trade_history.emplace_back(TradeReport{
                        .buy_order_id = buy_order->id,
                        .sell_order_id = id,
                        .price = best_bid,
                        .volume = trade_vol,
                        .symbol = book_symbol
                    });

                    if (buy_order->volume == 0) {
                        bid_level.pop_front();
                        order_pool.release(buy_order);
                    }
                }

                if (bid_level.empty()) {
                    best_bid--;
                    while (best_bid > 0 && bids[best_bid].empty()) {
                        best_bid--;
                    }
                }
            }
        }

        if (remaining_volume > 0 && type == OrderType::LIMIT) {
            Order* order = order_pool.acquire();
            order->reset(id, price, remaining_volume, side, type, book_symbol);

            if (side == Side::BUY) {
                bids[price].push_back(order);
                if (price > best_bid) best_bid = price;
            } else {
                asks[price].push_back(order);
                if (price < best_ask) best_ask = price;
            }
        }
    }

    std::vector<TradeReport>&& retrieve_trades() {
        return std::move(trade_history);
    }

    void print_depth_of_market() const {
        std::shared_lock<std::shared_mutex> lock(rw_mutex);

        std::cout << "\n===== Depth of Market =====\n";
        std::cout << "--- ASKS (Sell) ---\n";

        bool found_any = false;
        int count = 0;

        std::vector<std::pair<uint32_t, uint32_t>> ask_levels;
        for (uint32_t p = best_ask; p < max_price_limit && count < 10; ++p) {
            if (!asks[p].empty()) {
                uint32_t total_vol = 0;
                Order* curr = asks[p].head;
                while (curr) {
                    total_vol += curr->volume;
                    curr = curr->next;
                }
                ask_levels.push_back({p, total_vol});
                count++;
            }
        }

        for (auto it = ask_levels.rbegin(); it != ask_levels.rend(); ++it) {
            std::cout << "Price: " << it->first << "\t | Volume: " << it->second << "\n";
            found_any = true;
        }
        if (!found_any) std::cout << " (No offers)\n";

        std::cout << "---------------------------------------------\n";
        std::cout << "--- BIDS (Buy) ---\n";

        found_any = false;
        count = 0;
        for (uint32_t p = best_bid; p > 0 && count < 10; --p) {
            if (!bids[p].empty()) {
                uint32_t total_vol = 0;
                Order* curr = bids[p].head;
                while (curr) {
                    total_vol += curr->volume;
                    curr = curr->next;
                }
                std::cout << "Price: " << p << "\t | Volume: " << total_vol << "\n";
                count++;
                found_any = true;
            }
        }
        if (!found_any) std::cout << " (No offers)\n";
        std::cout << "=============================================\n";
    }

private:
    bool can_fully_fill(uint32_t limit_price, uint32_t required_vol, Side side) const {
        uint32_t accum_vol = 0;

        if (side == Side::BUY) {
            uint32_t curr_ask = best_ask;
            while (curr_ask <= limit_price && curr_ask < max_price_limit) {
                Order* order = asks[curr_ask].head;
                while (order) {
                    accum_vol += order->volume;
                    if (accum_vol >= required_vol) return true;
                    order = order->next;
                }
                curr_ask++;
            }
        } else {
            uint32_t curr_bid = best_bid;
            while (curr_bid >= limit_price && curr_bid > 0) {
                Order* order = bids[curr_bid].head;
                while (order) {
                    accum_vol += order->volume;
                    if (accum_vol >= required_vol) return true;
                    order = order->next;
                }
                curr_bid--;
            }
        }
        return false;
    }
};