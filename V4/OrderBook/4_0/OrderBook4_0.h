//
// Created by Curtis on 30/10/26.
//

#ifndef MATCHINGENGINE_ORDERBOOK4_0_H
#define MATCHINGENGINE_ORDERBOOK4_0_H
#include "MatchingEngine.h"
#include "../OrderBookTypes.h"

#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

namespace v4 {

class OrderBook4_0 : public MatchingEngine {
public:
    using L1 = MatchingEngine::L1;

    explicit OrderBook4_0(std::uint64_t anchor_price)
        : denseLow_(anchor_price >= denseWindowSide ? anchor_price - denseWindowSide : 0), // basically max(0, anchorPrice - denseWindowSide)
          denseBids_(2 * denseWindowSide + 1),
          denseAsks_(2 * denseWindowSide + 1),
          denseBidBitmap_(denseBids_.size()),
          denseAskBitmap_(denseAsks_.size()) {}

    L1 top() const override;

    void apply(const AddOrder& o) override;
    void apply(const CancelOrder& o) override;

private:
    struct OrderLocation {
        Side side;
        std::uint64_t price;
    };

    std::uint64_t denseLow_;
    std::vector<OrdersAtPrice> denseBids_;
    std::vector<OrdersAtPrice> denseAsks_;
    OccupancyBitmap denseBidBitmap_;
    OccupancyBitmap denseAskBitmap_;

    std::unordered_map<std::uint64_t, OrderLocation> order_locations;
    std::map<std::uint64_t, PriceBucket> bid_map;
    std::map<std::uint64_t, PriceBucket> ask_map;
    L1 l1_{};

    void update_l1(Side side);
    void handle_bid(const AddOrder& o);
    std::uint64_t match_bid_to_ask(const AddOrder& o);
    void handle_ask(const AddOrder& o);
    std::uint64_t match_ask_to_bid(const AddOrder& o);

    [[nodiscard]] bool price_in_dense(const std::uint64_t price) const {
        return price >= denseLow_ && price - denseLow_ < denseBids_.size();
    }
};

} // namespace v4

#endif //MATCHINGENGINE_ORDERBOOK4_0_H
