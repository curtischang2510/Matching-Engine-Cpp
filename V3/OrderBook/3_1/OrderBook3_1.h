//
// Created by Curtis on 30/10/26.
//

#ifndef MATCHINGENGINE_ORDERBOOK3_1_H
#define MATCHINGENGINE_ORDERBOOK3_1_H
#include "MatchingEngine.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include <vector>

namespace v3_1 {
inline constexpr std::uint64_t kTicksPerBucket = 1000;

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
    static constexpr std::uint64_t numWords = (kTicksPerBucket + 63) / 64;
    std::array<std::uint64_t, numWords> occupied{};

    void set(std::size_t i) {
        occupied[i / 64] |= std::uint64_t{1} << (i % 64);
    }

    void clear(std::size_t i) {
        occupied[i / 64] &= ~(std::uint64_t{1} << (i % 64));
    }

    bool empty() const {
        for (std::uint64_t word : occupied) {
            if (word != 0) return false;
        }
        return true;
    }

// #include <simd/simd.h>
//
//     bool empty() const {
//         simd::ulong2 combined = {0, 0};
//
//         for (std::size_t i = 0; i < occupied.size(); i += 2) {
//             simd::ulong2 pair = {occupied[i], occupied[i + 1]};
//             combined |= pair;
//         }
//
//         return (combined.x | combined.y) == 0;
//     }

    std::size_t first() const {
        for (std::size_t w = 0; w < numWords; ++w) {
            if (occupied[w] != 0) {
                return w * 64 + std::countr_zero(occupied[w]);
            }
        }
        return kTicksPerBucket; // since 0 indexed, this means empty
    }

    std::size_t last() const {
        for (std::size_t w = numWords; w > 0; --w) {
            const std::uint64_t word = occupied[w - 1];
            if (word != 0)
                return (w - 1) * 64 + (63 - std::countl_zero(word));
        }
        return kTicksPerBucket; // since 0 indexed, this means empty
    }
};

class OrderBook3_1 : public MatchingEngine {
public:
    using L1 = MatchingEngine::L1;

    L1 top() const override;

    void apply(const AddOrder& o) override;
    void apply(const CancelOrder& o) override;

private:
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

} // namespace v3_1

#endif //MATCHINGENGINE_ORDERBOOK3_1_H
