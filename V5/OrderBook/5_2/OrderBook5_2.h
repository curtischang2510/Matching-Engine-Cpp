//
// Created by Curtis on 30/10/26.
//

#ifndef MATCHINGENGINE_ORDERBOOK5_2_H
#define MATCHINGENGINE_ORDERBOOK5_2_H
#include "MatchingEngine.h"
#include "../OrderBookTypes.h"

#include <cstdint>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>
#include <boost/unordered/unordered_flat_map.hpp>

namespace v5 {

struct OrdersAtPrice5_2 {
    std::uint64_t price = 0;
    std::uint64_t quantity = 0;
    // The list owns resting orders; front is the oldest order at this price.
    std::list<Order> orders;
};

struct PriceBucket5_2 {
    std::vector<OrdersAtPrice5_2> levels;
    OccupancyBitmap occupied{kTicksPerBucket};
};

class OrderBook5_2 : public MatchingEngine {
public:
    using L1 = MatchingEngine::L1;

    explicit OrderBook5_2(std::uint64_t anchor_price)
        : denseLow_(anchor_price >= denseWindowSide ? anchor_price - denseWindowSide : 0), // basically max(0, anchorPrice - denseWindowSide)
          denseBids_(2 * denseWindowSide + 1),
          denseAsks_(2 * denseWindowSide + 1),
          denseBidBitmap_(denseBids_.size()),
          denseAskBitmap_(denseAsks_.size()) {
        // Try profiling with this line commented, then uncomment and try again!! Search rehash in profiler
        order_locations.reserve(5000);
        for (size_t i = 0; i < 50000; ++i) {
            free_order_nodes.emplace_back(0, 0, 0, 0);
        }
    }

    L1 top() const override;

    void apply(const AddOrder& o) override;
    void apply(const CancelOrder& o) override;

private:
    using BucketMap = std::map<std::uint64_t, PriceBucket5_2>;
    // An ID finds its list node directly, so cancellation does not scan the list.
    struct OrderLocation {
        Side side;
        OrdersAtPrice5_2* level; // Stable dense slot or tree price level.
        std::list<Order>::iterator order; // Stable until this order is erased.
        BucketMap::iterator bucket; // end() marks a dense price.
    };

    std::uint64_t denseLow_;
    std::vector<OrdersAtPrice5_2> denseBids_;
    std::vector<OrdersAtPrice5_2> denseAsks_;
    OccupancyBitmap denseBidBitmap_;
    OccupancyBitmap denseAskBitmap_;


    // std::unordered_map<std::uint64_t, OrderLocation> order_locations;
    boost::unordered_flat_map<std::uint64_t, OrderLocation> order_locations;
    std::list<Order> free_order_nodes;
    BucketMap bid_map;
    BucketMap ask_map;
    L1 l1_{};

    void update_l1(Side side);
    std::list<Order>::iterator rest_order(std::list<Order>& orders, const AddOrder& o,
                                          std::uint64_t quantity);
    void recycle_order(std::list<Order>& orders, std::list<Order>::iterator order);
    void handle_bid(const AddOrder& o);
    std::uint64_t match_bid_to_ask(const AddOrder& o);
    void handle_ask(const AddOrder& o);
    std::uint64_t match_ask_to_bid(const AddOrder& o);

    [[nodiscard]] bool price_in_dense(const std::uint64_t price) const {
        return price >= denseLow_ && price - denseLow_ < denseBids_.size();
    }
};

} // namespace v5

#endif //MATCHINGENGINE_ORDERBOOK5_2_H
