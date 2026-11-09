#if defined(ENGINE_VERSION_4_0)
#include "OrderBook/4_0/OrderBook4_0.h"
using SelectedBook = v4::OrderBook4_0;
#elif defined(ENGINE_VERSION_4_1)
#include "OrderBook/4_1/OrderBook4_1.h"
using SelectedBook = v4::OrderBook4_1;
#elif defined(ENGINE_VERSION_5_0)
#include "OrderBook/5_0/OrderBook5_0.h"
using SelectedBook = v5::OrderBook5_0;
#elif defined(ENGINE_VERSION_5_1)
#include "OrderBook/5_1/OrderBook5_1.h"
using SelectedBook = v5::OrderBook5_1;
#elif defined(ENGINE_VERSION_5_2)
#include "OrderBook/5_2/OrderBook5_2.h"
using SelectedBook = v5::OrderBook5_2;
#elif defined(ENGINE_VERSION_5_4)
#include "OrderBook/5_4/OrderBook5_4.h"
using SelectedBook = v5::OrderBook5_4;
#else
#error Select an engine version in CMake
#endif
#include "Orders/orderParser.h"
#include "SPSCBufferV3.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <latch>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>

namespace {

constexpr std::size_t kShardCount = 2;
constexpr std::size_t kActiveShard = 0;
constexpr std::size_t kWorkerCount = 1;
constexpr std::size_t kProfileMessageCount = 15'000'000;
constexpr std::uint64_t kAnchorPriceTicks = 50'000; // $50, with 1,000 ticks per dollar.
constexpr std::array<std::string_view, 6> kProfileTickers = {
    "AAPL", "MSFT", "NVDA", "GOOG", "TSLA", "AMZN"
};

struct RoutedOrder {
    std::string ticker;
    OrderMessage message;
};

struct Shard {
    SPSCBufferV3<RoutedOrder> queue{4096};
    std::unordered_map<std::string, std::unique_ptr<MatchingEngine>> books;
    std::uint64_t processed = 0;
    double worker_seconds = 0;
};

std::unique_ptr<MatchingEngine> make_engine(std::uint64_t anchor_price) {
    return std::make_unique<SelectedBook>(anchor_price);
}

} // namespace

int main(int argc, char* argv[]) {
    const bool generated_mode = argc == 1 || std::string_view(argv[1]) == "--generated";
    if ((generated_mode && argc > 3) || (!generated_mode && argc > 2)) {
        std::cerr << "usage: " << argv[0]
                  << " [--generated [15000000]] | input-file\n";
        return 1;
    }
    std::ifstream input;
    if (generated_mode) {
        if (argc >= 3) {
            std::size_t count = 0;
            try { count = parseNumber<std::size_t>(argv[2]); }
            catch (const std::exception&) {
                std::cerr << "invalid generated message count\n";
                return 1;
            }
            if (count != kProfileMessageCount) {
                std::cerr << "generated workload contains exactly 15000000 commands\n";
                return 1;
            }
        }
    } else {
        input.open(argv[1]);
        if (!input) {
            std::cerr << "cannot open " << argv[1] << '\n';
            return 1;
        }
    }

    std::array<Shard, kShardCount> shards;
    // Build the known books before starting the clock, as we would before market open.
    for (const std::string_view ticker : kProfileTickers) {
        shards[kActiveShard].books.emplace(std::string(ticker), make_engine(kAnchorPriceTicks));
    }
    std::array<std::jthread, kWorkerCount> workers;
    std::atomic<bool> done{false};
    std::latch workers_ready{kWorkerCount};

    for (std::size_t i = 0; i < kWorkerCount; ++i) {
        workers[i] = std::jthread([&, i] {
            Shard& shard = shards[i];
            RoutedOrder routed;
            workers_ready.count_down();
            std::chrono::steady_clock::time_point worker_start;
            while (true) {
                if (shard.queue.try_pop(routed)) {
                    if (shard.processed == 0) worker_start = std::chrono::steady_clock::now();
                    std::unique_ptr<MatchingEngine>& book = shard.books[routed.ticker];
                    if (!book) book = make_engine(kAnchorPriceTicks);
                    if (const AddOrder* add = std::get_if<AddOrder>(&routed.message)) {
                        book->apply(*add);
                    } else {
                        book->apply(std::get<CancelOrder>(routed.message));
                    }
                    ++shard.processed;
                } else if (done.load(std::memory_order_acquire)) {
                    break;
                }
            }
            if (shard.processed != 0) {
                shard.worker_seconds = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - worker_start).count();
            }
        });
    }

    std::unordered_map<std::uint64_t, std::string> ticker_by_id;
    std::uint64_t submitted = 0;
    std::uint64_t adds = 0;
    std::uint64_t cancels = 0;
    std::uint64_t first_cancel_at = 0;
    std::exception_ptr failure;
    workers_ready.wait();
    const auto start = std::chrono::steady_clock::now();

    try {
        if (generated_mode) {
            // 7.5M commands build and clear levels without cancels. In the
            // second 7.5M, each cancel targets one of 4,500 live orders.
            std::array<int, 4'500> cancel_offsets;
            std::iota(cancel_offsets.begin(), cancel_offsets.end(), 0);
            std::mt19937 rng{42};
            std::shuffle(cancel_offsets.begin(), cancel_offsets.end(), rng);
            std::uint64_t next_id = 1;
            auto submit = [&](const std::string& ticker, OrderMessage message) {
                const bool is_cancel = std::holds_alternative<CancelOrder>(message);
                const RoutedOrder routed{ticker, std::move(message)};
                while (!shards[kActiveShard].queue.try_push(routed)) {}
                ++submitted;
                if (is_cancel) {
                    if (first_cancel_at == 0) first_cancel_at = submitted;
                    ++cancels;
                }
                else ++adds;
            };

            for (int phase = 0; phase < 2; ++phase) {
                for (int round = 0; round < 1'500; ++round) {
                    const std::string ticker(kProfileTickers[round % kProfileTickers.size()]);
                    const int resting_count = phase == 0 ? 4'999 : 4'500;
                    const std::uint64_t first_id = next_id;
                    for (int i = 0; i < resting_count; ++i) {
                        submit(ticker, AddOrder{{0, next_id++}, BID, ticker, kAnchorPriceTicks, 1});
                    }
                    if (phase == 1) {
                        for (int i = 0; i < 499; ++i) {
                            submit(ticker, CancelOrder{{0, first_id + cancel_offsets[i]}});
                        }
                    }
                    // Clear the level so live memory stays bounded across rounds.
                    submit(ticker, AddOrder{{0, next_id++}, ASK, ticker, kAnchorPriceTicks,
                                             static_cast<std::uint32_t>(resting_count - (phase == 1 ? 499 : 0))});
                }
            }
        } else {
            std::string line;
            while (std::getline(input, line)) {
                if (line.empty()) continue;
                OrderMessage message = parseOrder(line);
                std::string ticker;
                if (const AddOrder* add = std::get_if<AddOrder>(&message)) {
                    ticker = add->ticker;
                    if (!ticker_by_id.emplace(add->header.order_id, ticker).second) {
                        throw std::runtime_error("duplicate order ID");
                    }
                    ++adds;
                } else {
                    const CancelOrder& cancel = std::get<CancelOrder>(message);
                    const auto it = ticker_by_id.find(cancel.header.order_id);
                    if (it == ticker_by_id.end()) throw std::runtime_error("unknown cancel ID");
                    ticker = it->second;
                    ++cancels;
                }

                const RoutedOrder routed{ticker, std::move(message)};
                while (!shards[kActiveShard].queue.try_push(routed)) {}
                ++submitted;
            }
            if (input.bad()) throw std::runtime_error("failed while reading input file");
        }
    } catch (...) {
        failure = std::current_exception();
    }

    done.store(true, std::memory_order_release);
    for (std::jthread& worker : workers) worker.join();
    if (failure) {
        try { std::rethrow_exception(failure); }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
        return 1;
    }

    const double wall_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    const double seconds = shards[kActiveShard].worker_seconds;
    std::uint64_t processed = 0;
    std::size_t books = 0;
    std::uint64_t best_bid_qty = 0;
    std::uint64_t best_ask_qty = 0;
    for (const Shard& shard : shards) {
        processed += shard.processed;
        books += shard.books.size();
        for (const auto& entry : shard.books) {
            const MatchingEngine::L1 top = entry.second->top();
            best_bid_qty += top.highest_bid_qty;
            best_ask_qty += top.lowest_ask_qty;
        }
    }
    std::cout << "messages=" << processed
              << " adds=" << adds << " cancels=" << cancels
              << " books=" << books
              << " shard0=" << shards[0].processed
              << " shard1=" << shards[1].processed
              << " best_bid_qty=" << best_bid_qty
              << " best_ask_qty=" << best_ask_qty
              << " seconds=" << seconds
              << " wall_seconds=" << wall_seconds
              << " messages/s=" << (seconds > 0 ? static_cast<std::uint64_t>(processed / seconds) : 0);
    if (generated_mode) std::cout << " first_cancel_at=" << first_cancel_at;
    std::cout << '\n';
    return processed == submitted ? 0 : 1;
}
