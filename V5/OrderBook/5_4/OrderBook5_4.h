//
// Created by Curtis on 30/10/26.
//

#ifndef MATCHINGENGINE_ORDERBOOK5_4_H
#define MATCHINGENGINE_ORDERBOOK5_4_H
#include "MatchingEngine.h"
#include "../OrderBookTypes.h"

#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

namespace v5 {

struct OrderNode5_4;

// A price level links orders in arrival order; the ID map owns the nodes.
struct OrdersAtPrice5_4 {
    std::uint64_t price = 0;
    std::uint64_t quantity = 0;
    OrderNode5_4* head = nullptr;
    OrderNode5_4* tail = nullptr;

    [[nodiscard]] bool empty() const { return head == nullptr; }
    void push_back(OrderNode5_4* node);
    void erase(OrderNode5_4* node);
};

struct PriceBucket5_4 {
    std::vector<OrdersAtPrice5_4> levels;
    OccupancyBitmap occupied{kTicksPerBucket};
};

using BucketMap5_4 = std::map<std::uint64_t, PriceBucket5_4>;

struct OrderNode5_4 {
    Order order;
    // Links live in this node, rather than in a separate std::list node.
    OrderNode5_4* prev = nullptr;
    OrderNode5_4* next = nullptr;
    OrdersAtPrice5_4* level = nullptr; // Back-reference for direct cancellation.
    Side side;
    BucketMap5_4::iterator bucket; // end() marks a dense price.
};

class OrderBook5_4 : public MatchingEngine {
public:
    using L1 = MatchingEngine::L1;

    explicit OrderBook5_4(std::uint64_t anchor_price)
        : denseLow_(anchor_price >= denseWindowSide ? anchor_price - denseWindowSide : 0), // basically max(0, anchorPrice - denseWindowSide)
          denseBids_(2 * denseWindowSide + 1),
          denseAsks_(2 * denseWindowSide + 1),
          denseBidBitmap_(denseBids_.size()),
          denseAskBitmap_(denseAsks_.size()) {}

    L1 top() const override;

    void apply(const AddOrder& o) override;
    void apply(const CancelOrder& o) override;

private:
    std::uint64_t denseLow_;
    std::vector<OrdersAtPrice5_4> denseBids_;
    std::vector<OrdersAtPrice5_4> denseAsks_;
    OccupancyBitmap denseBidBitmap_;
    OccupancyBitmap denseAskBitmap_;

    // The ID map owns each node; its address remains stable across rehashes.
    std::unordered_map<std::uint64_t, OrderNode5_4> order_nodes;
    BucketMap5_4 bid_map;
    BucketMap5_4 ask_map;
    L1 l1_{};

    void update_l1(Side side);
    void rest_order(OrdersAtPrice5_4& level, const AddOrder& order,
                    std::uint64_t quantity, BucketMap5_4::iterator bucket);
    void handle_bid(const AddOrder& o);
    std::uint64_t match_bid_to_ask(const AddOrder& o);
    void handle_ask(const AddOrder& o);
    std::uint64_t match_ask_to_bid(const AddOrder& o);

    [[nodiscard]] bool price_in_dense(const std::uint64_t price) const {
        return price >= denseLow_ && price - denseLow_ < denseBids_.size();
    }
};

} // namespace v5

#endif //MATCHINGENGINE_ORDERBOOK5_4_H
