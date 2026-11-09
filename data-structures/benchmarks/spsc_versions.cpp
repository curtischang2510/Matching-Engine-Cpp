#include "SPSCBufferV3.h"
#include "SPSCBufferV1.h"
#include "SPSCBufferV2.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <latch>
#include <thread>

template <typename Queue>
double run(std::uint64_t count) {
    alignas(128) Queue queue{4096};
    std::latch ready{2};
    std::latch go{1};
    std::uint64_t sum = 0;
    std::chrono::steady_clock::time_point finished;

    std::jthread producer([&] {
        ready.count_down();
        go.wait();
        for (std::uint64_t value = 1; value <= count; ++value) {
            while (!queue.try_push(value)) {}
        }
    });
    std::jthread consumer([&] {
        ready.count_down();
        go.wait();
        for (std::uint64_t i = 0; i < count; ++i) {
            std::uint64_t value;
            while (!queue.try_pop(value)) {}
            sum += value;
        }
        finished = std::chrono::steady_clock::now();
    });

    ready.wait();
    const auto started = std::chrono::steady_clock::now();
    go.count_down();
    producer.join();
    consumer.join();
    if (sum != count * (count + 1) / 2) std::terminate();
    return std::chrono::duration<double>(finished - started).count();
}

int main() {
    constexpr std::uint64_t count = 50'000'000;
    constexpr int rounds = 5;
    std::array<std::array<double, rounds>, 3> times{};

    for (int round = 0; round < rounds; ++round) {
        // Repeat the earlier padded/unpadded comparison, alternating who runs first.
        if (round % 2 == 0) {
            times[0][round] = run<SPSCBufferV1<std::uint64_t>>(count);
            times[1][round] = run<SPSCBufferV2<std::uint64_t>>(count);
        } else {
            times[1][round] = run<SPSCBufferV2<std::uint64_t>>(count);
            times[0][round] = run<SPSCBufferV1<std::uint64_t>>(count);
        }
        std::cout << std::fixed << std::setprecision(3)
                  << "round=" << round + 1
                  << " v1=" << times[0][round]
                  << " v2=" << times[1][round] << " seconds\n";
    }

    for (int round = 0; round < rounds; ++round) {
        times[2][round] = run<SPSCBufferV3<std::uint64_t>>(count);
        std::cout << "round=" << round + 1 << " v3=" << times[2][round] << " seconds\n";
    }

    for (auto& version : times) std::sort(version.begin(), version.end());
    std::cout << "median v1=" << times[0][rounds / 2]
              << " v2=" << times[1][rounds / 2]
              << " v3=" << times[2][rounds / 2] << " seconds\n";
}
