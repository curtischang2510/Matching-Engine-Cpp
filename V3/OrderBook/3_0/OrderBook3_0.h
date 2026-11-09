//
// Created by Curtis on 30/10/26.
//

#ifndef MATCHINGENGINE_ORDERBOOK3_0_H
#define MATCHINGENGINE_ORDERBOOK3_0_H
#include "MatchingEngine.h"

#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include <vector>

// Orderbook for a single stock ticker
struct Order {
    std::uint64_t id;
    std::uint64_t price;
    std::uint64_t quantity;
    std::uint64_t timestamp;
};

struct OrdersAtPrice {
    std::uint64_t price;
    std::uint64_t quantity = 0;
    std::deque<Order> orders;
};

struct PriceBucket {
    std::vector<OrdersAtPrice> levels;
};

class OrderBook3_0 : public MatchingEngine {
public:
    using L1 = MatchingEngine::L1;

    L1 top() const override;

    void apply(const AddOrder& o) override;
    void apply(const CancelOrder& o) override;

private:
    static constexpr std::uint64_t kTicksPerBucket = 1000;

    struct OrderLocation {
        Side side;
        std::uint64_t price;
    };

    std::unordered_map<std::uint64_t, OrderLocation> order_locations;
    std::map<std::uint64_t, PriceBucket> bid_map;
    std::map<std::uint64_t, PriceBucket> ask_map;
    L1 l1_{};
    void update_l1(Side side);
    void handle_bid(const AddOrder& o);
    std::uint64_t match_bid_to_ask(const AddOrder& o);
    void handle_ask(const AddOrder& o);
    std::uint64_t match_ask_to_bid(const AddOrder& o);
};

#endif //MATCHINGENGINE_ORDERBOOK3_0_H
