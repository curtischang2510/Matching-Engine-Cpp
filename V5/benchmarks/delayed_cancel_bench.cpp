#include "OrderBook/4_0/OrderBook4_0.h"
#include "OrderBook/5_0/OrderBook5_0.h"
#include "OrderBook/5_1/OrderBook5_1.h"
#include "OrderBook/5_2/OrderBook5_2.h"
#include "OrderBook/5_4/OrderBook5_4.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string_view>

template <typename Book>
void run() {
    std::array<std::unique_ptr<Book>, 6> books;
    for (std::unique_ptr<Book>& book : books) book = std::make_unique<Book>(50'000);

    std::array<int, 4'500> cancel_offsets;
    std::iota(cancel_offsets.begin(), cancel_offsets.end(), 0);
    std::mt19937 rng{42};
    std::shuffle(cancel_offsets.begin(), cancel_offsets.end(), rng);

    AddOrder bid{{0, 0}, BID, "TEST", 50'000, 1};
    CancelOrder cancel{{0, 0}};
    AddOrder ask{{0, 0}, ASK, "TEST", 50'000, 0};
    std::uint64_t next_id = 1;
    const auto start = std::chrono::steady_clock::now();
    for (int phase = 0; phase < 2; ++phase) {
        for (int round = 0; round < 1'500; ++round) {
            Book& book = *books[round % books.size()];
            const int resting_count = phase == 0 ? 4'999 : 4'500;
            const std::uint64_t first_id = next_id;
            for (int i = 0; i < resting_count; ++i) {
                bid.header.order_id = next_id++;
                book.apply(bid);
            }
            if (phase == 1) {
                for (int i = 0; i < 499; ++i) {
                    cancel.header.order_id = first_id + cancel_offsets[i];
                    book.apply(cancel);
                }
                if (book.top().highest_bid_qty != 4'001)
                    throw std::runtime_error("cancellation did not remove a live order");
            }
            ask.header.order_id = next_id++;
            ask.quantity = resting_count - (phase == 1 ? 499 : 0);
            book.apply(ask);
            if (book.top().highest_bid || book.top().lowest_ask)
                throw std::runtime_error("batch did not clear the book");
        }
    }
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "messages=15000000 adds=14251500 cancels=748500 seconds=" << seconds << '\n';
}

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;
    const std::string_view version{argv[1]};
    if (version == "4.0") run<v4::OrderBook4_0>();
    else if (version == "5.0") run<v5::OrderBook5_0>();
    else if (version == "5.1") run<v5::OrderBook5_1>();
    else if (version == "5.2") run<v5::OrderBook5_2>();
    else if (version == "5.4") run<v5::OrderBook5_4>();
    else return 1;
}
