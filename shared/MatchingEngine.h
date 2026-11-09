#ifndef MATCHINGENGINE_MATCHINGENGINE_H
#define MATCHINGENGINE_MATCHINGENGINE_H

#include "Orders/orders.h"

#include <cstdint>
#include <optional>

class MatchingEngine {
public:
    struct L1 {
        std::optional<std::uint64_t> highest_bid;
        std::uint64_t highest_bid_qty = 0;
        std::optional<std::uint64_t> lowest_ask;
        std::uint64_t lowest_ask_qty = 0;
    };

    virtual ~MatchingEngine() = default;
    virtual void apply(const AddOrder& order) = 0;
    virtual void apply(const CancelOrder& order) = 0;
    virtual L1 top() const = 0;
};

#endif // MATCHINGENGINE_MATCHINGENGINE_H
