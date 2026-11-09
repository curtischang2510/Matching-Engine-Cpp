#ifndef MATCHINGENGINE_ORDERBOOK4_TYPES_H
#define MATCHINGENGINE_ORDERBOOK4_TYPES_H

#include "OccupancyBitmap.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace v4 {
inline constexpr std::uint64_t kTicksPerBucket = 1000;
inline constexpr std::uint64_t denseWindowSide = 10 * 1000; // $10 window on both sides of anchor price

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
    OccupancyBitmap occupied{kTicksPerBucket};
};
} // namespace v4

#endif // MATCHINGENGINE_ORDERBOOK4_TYPES_H
