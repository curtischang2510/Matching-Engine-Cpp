#ifndef MATCHINGENGINE_ORDERBOOK5_TYPES_H
#define MATCHINGENGINE_ORDERBOOK5_TYPES_H

#include "OccupancyBitmap.h"

#include <cstdint>

namespace v5 {
inline constexpr std::uint64_t kTicksPerBucket = 1000;
inline constexpr std::uint64_t denseWindowSide = 10 * 1000;

struct Order {
    std::uint64_t id;
    std::uint64_t price;
    std::uint64_t quantity;
    std::uint64_t timestamp;
};
} // namespace v5

#endif // MATCHINGENGINE_ORDERBOOK5_TYPES_H
