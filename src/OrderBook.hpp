#pragma once
#include "Order.hpp"
#include "ObjectPool.hpp"
#include <vector>
#include <iostream>
#include <algorithm>

class OrderBook {
private:
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
        
        bool empty() const { return head == nullptr; }
    };

    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;

    uint32_t best_bid = 0;
    uint32_t best_ask;

    std::vector<TradeReport> trade_history;

    ObjectPool<Order>& order_pool;
    std::string_view book_symbol;

public:
    explicit OrderBook(ObjectPool<Order>& pool, std::string_view symbol, size_t max_price = 100000)
        : bids(max_price), asks(max_price), best_ask(max_price - 1), order_pool(pool), book_symbol(symbol) {
        trade_history.reserve(10000);
    }

    template <ValidNumericType T_ID, ValidNumericType T_Price, ValidNumericType T_Vol>
    void add_order(T_ID id, T_Price price, T_Vol volume, Side side) {
        Order* order = order_pool.acquire();
        order->reset(id, price, volume, side, book_symbol);

        if (side == Side::BUY) {
            bids[price].push_back(order);
            if (price > best_bid) best_bid = price;
        } else {
            asks[price].push_back(order);
            if (price < best_ask) best_ask = price;
        }

        match();
    }

    std::vector<TradeReport>&& retrieve_trades() {
        return std::move(trade_history);
    }

private:
    void match() {
        while (best_bid >= best_ask && best_bid > 0) {
            PriceLevel& bid_level = bids[best_bid];
            PriceLevel& ask_level = asks[best_ask];

            while (!bid_level.empty() && !ask_level.empty()) {
                Order* buy_order = bid_level.head;
                Order* sell_order = ask_level.head;

                uint32_t trade_volume = std::min(buy_order->volume, sell_order->volume);
                
                buy_order->volume -= trade_volume;
                sell_order->volume -= trade_volume;

                trade_history.emplace_back(TradeReport{
                    .buy_order_id = buy_order->id,
                    .sell_order_id = sell_order->id,
                    .price = best_ask,
                    .volume = trade_volume,
                    .symbol = book_symbol
                });

                if (buy_order->volume == 0) {
                    bid_level.pop_front();
                    order_pool.release(buy_order);
                }
                if (sell_order->volume == 0) {
                    ask_level.pop_front();
                    order_pool.release(sell_order);
                }
            }

            if (bid_level.empty()) {
                while (best_bid > 0 && bids[best_bid].empty()) {
                    best_bid--;
                }
            }
            if (ask_level.empty()) {
                while (best_ask < asks.size() - 1 && asks[best_ask].empty()) {
                    best_ask++;
                }
            }
        }
    }
};