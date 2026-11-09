//
// Created by Curtis on 30/10/26.
//

#ifndef MATCHINGENGINE_ORDERS_H
#define MATCHINGENGINE_ORDERS_H
#include <cstdint>
#include <string>
#include <variant>

enum Side {
    BID,
    ASK
};

struct OrderHeader {
    std::uint64_t timestamp;
    std::uint64_t order_id;
};

struct AddOrder {
    OrderHeader header;
    Side side;
    std::string ticker;
    std::uint64_t price_ticks;
    std::uint32_t quantity;
};

struct CancelOrder {
    OrderHeader header;
};

using OrderMessage = std::variant<AddOrder, CancelOrder>;

#endif //MATCHINGENGINE_ORDERS_H
