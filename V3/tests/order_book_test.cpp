#include "OrderBook/3_0/OrderBook3_0.h"
#include "Orders/orderParser.h"

#include <cassert>
#include <cstdint>

namespace {

AddOrder add(std::uint64_t id, Side side, std::uint64_t price, std::uint32_t quantity) {
    return {{id, id}, side, "TEST", price, quantity};
}

CancelOrder cancel(std::uint64_t id) {
    return {{id, id}};
}

void test_ask_priority_and_fifo() {
    OrderBook3_0 book;
    assert(!book.top().lowest_ask);

    book.apply(add(1, ASK, 1001, 2));
    book.apply(add(2, ASK, 999, 3));
    book.apply(add(3, ASK, 999, 4));
    assert(book.top().lowest_ask == 999);
    assert(book.top().lowest_ask_qty == 7);

    book.apply(add(4, BID, 999, 3)); // The oldest ask at 999 fills first.
    assert(book.top().lowest_ask == 999);
    assert(book.top().lowest_ask_qty == 4);
    book.apply(cancel(1)); // A non-best cancel leaves the best ask alone.
    assert(book.top().lowest_ask == 999);
    book.apply(cancel(2)); // Already filled: must not cancel order 3.
    assert(book.top().lowest_ask_qty == 4);
    book.apply(cancel(3));
    assert(!book.top().lowest_ask);
    assert(book.top().lowest_ask_qty == 0);
    assert(!book.top().highest_bid); // Fully filled incoming bid did not rest.
}

void test_bid_priority_and_bucket_boundaries() {
    OrderBook3_0 book;
    book.apply(add(10, BID, 999, 2));
    book.apply(add(11, BID, 1000, 3));
    book.apply(add(12, BID, 2000, 4));
    assert(book.top().highest_bid == 2000);
    assert(book.top().highest_bid_qty == 4);

    book.apply(add(13, ASK, 1000, 5)); // Fill 2000, then one unit at 1000.
    assert(book.top().highest_bid == 1000);
    assert(book.top().highest_bid_qty == 2);
    assert(!book.top().lowest_ask);

    book.apply(add(14, ASK, 1001, 1)); // Cannot cross the remaining 1000 bid.
    assert(book.top().highest_bid == 1000);
    assert(book.top().lowest_ask == 1001);
    book.apply(cancel(11));
    assert(book.top().highest_bid == 999);
    assert(book.top().highest_bid_qty == 2);
    book.apply(cancel(14));
    assert(!book.top().lowest_ask);
    book.apply(cancel(10));
    assert(!book.top().highest_bid);
    assert(book.top().highest_bid_qty == 0);
}

void test_partial_fill_and_same_price_cancel() {
    OrderBook3_0 book;
    book.apply(add(20, ASK, 1000, 5));
    book.apply(add(21, ASK, 1000, 2));
    book.apply(add(22, BID, 1000, 3));
    assert(book.top().lowest_ask == 1000);
    assert(book.top().lowest_ask_qty == 4);
    book.apply(cancel(20)); // Cancels only its two remaining units.
    assert(book.top().lowest_ask_qty == 2);
    book.apply(add(23, BID, 1000, 4)); // Two fill; two rest as a bid.
    assert(!book.top().lowest_ask);
    assert(book.top().highest_bid == 1000);
    assert(book.top().highest_bid_qty == 2);
    book.apply(cancel(23));
    assert(!book.top().highest_bid);
}

} // namespace

int main() {
    const AddOrder parsed = std::get<AddOrder>(parseOrder("ADD,42,TEST,BID,101.25,7"));
    assert(parsed.header.order_id == 42 && parsed.price_ticks == 101250);
    assert(std::get<CancelOrder>(parseOrder("CANCEL,42")).header.order_id == 42);
    test_ask_priority_and_fifo();
    test_bid_priority_and_bucket_boundaries();
    test_partial_fill_and_same_price_cancel();
}
