#include "OrderBook/3_0/OrderBook3_0.h"
#include "OrderBook/3_1/OrderBook3_1.h"
#include "Orders/orderParser.h"
#include "SPSCBufferV3.h"

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
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>

#ifndef DEFAULT_ENGINE_VERSION
#define DEFAULT_ENGINE_VERSION "3.0"
#endif

namespace {

constexpr std::size_t kShardCount = 2;
constexpr std::size_t kActiveShard = 0;
constexpr std::size_t kWorkerCount = 1;

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

std::unique_ptr<MatchingEngine> make_engine(std::string_view version) {
    if (version == "3.0") return std::make_unique<OrderBook3_0>();
    if (version == "3.1") return std::make_unique<v3_1::OrderBook3_1>();
    return nullptr;
}

} // namespace

int main(int argc, char* argv[]) {
    constexpr const char* kInputPath = "/Users/curtis/Desktop/Projects/Matching-Engine-Cpp/test-data/profile_data.txt";
    if (argc > 3) {
        std::cerr << "usage: " << argv[0] << " [input-file] [engine-version]\n";
        return 1;
    }
    const char* input_path = argc >= 2 ? argv[1] : kInputPath;
    const std::string_view engine_version = argc == 3 ? argv[2] : DEFAULT_ENGINE_VERSION;
    if (engine_version != "3.0" && engine_version != "3.1") {
        std::cerr << "unknown engine version: " << engine_version << '\n';
        return 1;
    }
    std::ifstream input(input_path);
    if (!input) {
        std::cerr << "cannot open " << input_path << '\n';
        return 1;
    }

    std::array<Shard, kShardCount> shards;
    std::array<std::jthread, kWorkerCount> workers;
    std::atomic<bool> done{false};
    std::latch workers_ready{kWorkerCount};

    for (std::size_t i = 0; i < kWorkerCount; ++i) {
        workers[i] = std::jthread([&, i] {
            Shard& shard = shards[i];
            RoutedOrder routed;
            workers_ready.count_down();
            const auto worker_start = std::chrono::steady_clock::now();
            while (true) {
                if (shard.queue.try_pop(routed)) {
                    std::unique_ptr<MatchingEngine>& book = shard.books[routed.ticker];
                    if (!book) book = make_engine(engine_version);
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
            shard.worker_seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - worker_start).count();
        });
    }

    std::unordered_map<std::uint64_t, std::string> ticker_by_id;
    std::uint64_t submitted = 0;
    std::exception_ptr failure;
    workers_ready.wait();
    const auto start = std::chrono::steady_clock::now();

    try {
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
            } else {
                const CancelOrder& cancel = std::get<CancelOrder>(message);
                const auto it = ticker_by_id.find(cancel.header.order_id);
                if (it == ticker_by_id.end()) throw std::runtime_error("unknown cancel ID");
                ticker = it->second;
            }

            const RoutedOrder routed{ticker, std::move(message)};
            while (!shards[kActiveShard].queue.try_push(routed)) {}
            ++submitted;
        }
        if (input.bad()) throw std::runtime_error("failed while reading input file");
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

    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
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
    std::cout << "messages=" << processed << " books=" << books
              << " shard0=" << shards[0].processed
              << " shard1=" << shards[1].processed
              << " best_bid_qty=" << best_bid_qty
              << " best_ask_qty=" << best_ask_qty
              << " seconds=" << seconds
              << " messages/s=" << static_cast<std::uint64_t>(processed / seconds);
    for (std::size_t i = 0; i < kWorkerCount; ++i) {
        std::cout << " worker" << i << "_seconds=" << shards[i].worker_seconds;
    }
    std::cout << '\n';
    return processed == submitted ? 0 : 1;
}
