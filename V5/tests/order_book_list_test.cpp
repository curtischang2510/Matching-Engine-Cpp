#include "OrderBook/4_0/OrderBook4_0.h"
#include "OrderBook/5_0/OrderBook5_0.h"
#include "OrderBook/5_1/OrderBook5_1.h"
#include "OrderBook/5_2/OrderBook5_2.h"
#include "OrderBook/5_4/OrderBook5_4.h"

#include <cstdint>
#include <iostream>
#include <random>

template <typename Book>
bool check_fifo_and_cancel() {
    Book book{100'000};
    book.apply(AddOrder{{0, 1}, BID, "TEST", 100'000, 2});
    book.apply(AddOrder{{0, 2}, BID, "TEST", 100'000, 5});
    book.apply(AddOrder{{0, 3}, BID, "TEST", 100'000, 7});
    book.apply(CancelOrder{{0, 2}}); // Remove the middle node.
    if (book.top().highest_bid_qty != 9) return false;
    book.apply(AddOrder{{0, 4}, ASK, "TEST", 100'000, 3});
    if (book.top().highest_bid_qty != 6) return false; // IDs 1, then 3 matched.
    book.apply(CancelOrder{{0, 1}}); // Fully filled: no-op.
    book.apply(CancelOrder{{0, 3}});
    if (book.top().highest_bid || book.top().highest_bid_qty != 0) return false;

    book.apply(AddOrder{{0, 5}, ASK, "TEST", 120'000, 2}); // Tree price.
    book.apply(AddOrder{{0, 6}, ASK, "TEST", 120'000, 5});
    book.apply(AddOrder{{0, 7}, ASK, "TEST", 120'000, 7});
    book.apply(CancelOrder{{0, 6}});
    if (book.top().lowest_ask_qty != 9) return false;
    book.apply(AddOrder{{0, 8}, BID, "TEST", 120'000, 3});
    if (book.top().lowest_ask_qty != 6) return false;
    book.apply(CancelOrder{{0, 7}});
    return !book.top().lowest_ask && book.top().lowest_ask_qty == 0;
}

bool same_top(const MatchingEngine::L1& a, const MatchingEngine::L1& b) {
    return a.highest_bid == b.highest_bid && a.highest_bid_qty == b.highest_bid_qty
        && a.lowest_ask == b.lowest_ask && a.lowest_ask_qty == b.lowest_ask_qty;
}

int main() {
    if (!check_fifo_and_cancel<v5::OrderBook5_0>() ||
        !check_fifo_and_cancel<v5::OrderBook5_1>() ||
        !check_fifo_and_cancel<v5::OrderBook5_2>() ||
        !check_fifo_and_cancel<v5::OrderBook5_4>()) return 1;

    v4::OrderBook4_0 baseline{100'000};
    v5::OrderBook5_0 list{100'000};
    v5::OrderBook5_1 list_next{100'000};
    v5::OrderBook5_2 list_next_2{100'000};
    v5::OrderBook5_4 intrusive{100'000};
    std::mt19937_64 rng{42};
    std::uint64_t next_id = 1;
    for (int i = 0; i < 10'000; ++i) {
        if (next_id > 1 && rng() % 5 < 2) {
            const CancelOrder cancel{{0, 1 + rng() % (next_id - 1)}};
            baseline.apply(cancel);
            list.apply(cancel);
            list_next.apply(cancel);
            list_next_2.apply(cancel);
            intrusive.apply(cancel);
        } else {
            const std::uint64_t price = 75'000 + (rng() % 51) * 1'000;
            const AddOrder add{{0, next_id++}, rng() % 2 ? BID : ASK, "TEST",
                               price, static_cast<std::uint32_t>(1 + rng() % 20)};
            baseline.apply(add);
            list.apply(add);
            list_next.apply(add);
            list_next_2.apply(add);
            intrusive.apply(add);
        }
        if (!same_top(baseline.top(), list.top()) ||
            !same_top(baseline.top(), list_next.top()) ||
            !same_top(baseline.top(), list_next_2.top()) ||
            !same_top(baseline.top(), intrusive.top())) {
            std::cerr << "L1 mismatch after command " << i << '\n';
            return 1;
        }
    }
}
