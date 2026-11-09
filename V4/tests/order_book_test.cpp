#include "OrderBook/4_0/OrderBook4_0.h"

int main() {
    OccupancyBitmap bitmap{130};
    if (!bitmap.empty()) return 1;
    bitmap.set(64);
    bitmap.set(64); // A second order at the same price must not count twice.
    bitmap.clear(64);
    if (!bitmap.empty()) return 1;
    bitmap.clear(64); // Clearing an empty price must not underflow the count.
    if (!bitmap.empty()) return 1;

    OccupancyBitmap wide{1'100};
    if (wide.first() != 1'100) return 1;
    wide.set(900); // The SIMD scan must skip a full group of eight empty words.
    wide.set(70);
    if (wide.first() != 70 || wide.last() != 900) return 1;
    wide.clear(70);
    if (wide.first() != 900) return 1;
    wide.clear(900);
    if (!wide.empty() || wide.first() != 1'100) return 1;
    wide.set(70); // The reverse scan must also handle the final short group.
    if (wide.last() != 70) return 1;
    wide.clear(70);

    v4::OrderBook4_0 book{100000};
    book.apply(AddOrder{{0, 1}, Side::BID, "TEST", 80000, 2});  // Tree bid below the window.
    book.apply(AddOrder{{0, 2}, Side::BID, "TEST", 100000, 5}); // Dense bid.
    book.apply(AddOrder{{0, 3}, Side::ASK, "TEST", 80000, 1});
    if (book.top().highest_bid != 100000 || book.top().highest_bid_qty != 4) return 1;

    book.apply(AddOrder{{0, 4}, Side::BID, "TEST", 120000, 2}); // Tree bid above the window.
    book.apply(AddOrder{{0, 5}, Side::ASK, "TEST", 90000, 3});
    if (book.top().highest_bid != 100000 || book.top().highest_bid_qty != 3) return 1;

    book.apply(AddOrder{{0, 6}, Side::ASK, "TEST", 105000, 4}); // Resting dense ask.
    book.apply(AddOrder{{0, 7}, Side::BID, "TEST", 105000, 1});
    if (book.top().lowest_ask != 105000 || book.top().lowest_ask_qty != 3) return 1;

    book.apply(CancelOrder{{0, 2}}); // Dense bid: expose the lower tree bid.
    if (book.top().highest_bid != 80000 || book.top().highest_bid_qty != 2) return 1;
    book.apply(CancelOrder{{0, 6}}); // Dense ask: no asks remain.
    if (book.top().lowest_ask) return 1;
    book.apply(CancelOrder{{0, 1}}); // Tree bid: no bids remain.
    if (book.top().highest_bid) return 1;
    book.apply(CancelOrder{{0, 4}}); // A filled order is already gone.

    v4::OrderBook4_0 asks{100000};
    asks.apply(AddOrder{{0, 8}, Side::ASK, "TEST", 80000, 1});
    asks.apply(AddOrder{{0, 9}, Side::ASK, "TEST", 100000, 2});
    asks.apply(AddOrder{{0, 10}, Side::ASK, "TEST", 120000, 1});
    asks.apply(AddOrder{{0, 11}, Side::BID, "TEST", 130000, 1});
    if (asks.top().lowest_ask != 100000 || asks.top().lowest_ask_qty != 2) return 1;
    asks.apply(AddOrder{{0, 12}, Side::BID, "TEST", 130000, 2});
    if (asks.top().lowest_ask != 120000 || asks.top().lowest_ask_qty != 1) return 1;
    asks.apply(CancelOrder{{0, 10}}); // Tree ask: remove the final ask.
    if (asks.top().lowest_ask) return 1;
}
