#ifndef MATCHINGENGINE_OCCUPANCYBITMAP_H
#define MATCHINGENGINE_OCCUPANCYBITMAP_H

#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <simd/simd.h>

class OccupancyBitmap {
public:
    explicit OccupancyBitmap(std::size_t bit_count)
        : bit_count_(bit_count), words_((bit_count + 63) / 64, 0) {}

    void set(std::size_t i) {
        std::uint64_t& word = words_[i / 64];
        const std::uint64_t mask = std::uint64_t{1} << (i % 64);
        // if ((word & mask) == 0) {
        //     word |= mask;
        //     ++occupied_count;
        // }
        occupied_count += (word & mask) == 0;
        word |= mask;
    }

    void clear(std::size_t i) {
        std::uint64_t& word = words_[i / 64];
        const std::uint64_t mask = std::uint64_t{1} << (i % 64);
        // significant branch mispredictions
        // if (word & mask) {
        //     word &= ~mask;
        //     --occupied_count;
        // }
        occupied_count -= (word & mask) != 0;      // subtracts 0 or 1
        word = word & ~mask;
    }

    bool empty() const {
        return occupied_count == 0;
    }

    std::size_t first() const {
        if (occupied_count == 0) {
            return bit_count_;
        }
        for (std::size_t w = 0; w < words_.size(); ++w) {
            const std::uint64_t word = words_[w];
            if (word != 0)
                return w * 64 + std::countr_zero(word);
        }
        return bit_count_;
    }

    std::size_t last() const {
        if (occupied_count == 0) {
            return bit_count_;
        }

        for (std::size_t w = words_.size(); w > 0; --w) {
            const std::uint64_t word = words_[w - 1];
            if (word != 0)
                return (w - 1) * 64 + (63 - std::countl_zero(word));
        }
        return bit_count_;
    }

    std::size_t simd_first() const {
        if (occupied_count == 0) return bit_count_;

        std::size_t w = 0;

        for (; w + 8 <= words_.size(); w += 8) {
            simd::ulong8 group;
            std::memcpy(&group, words_.data() + w, sizeof(group));

            if (simd_reduce_max(group) == 0) continue;

            for (std::size_t lane = 0; lane < 8; ++lane) {
                const std::uint64_t word = group[lane];
                if (word != 0)
                    return (w + lane) * 64 + std::countr_zero(word);
            }
        }

        for (; w < words_.size(); ++w) {
            const std::uint64_t word = words_[w];
            if (word != 0)
                return w * 64 + std::countr_zero(word);
        }
        return bit_count_;
    }

    std::size_t simd_last() const {
        if (occupied_count == 0) return bit_count_;

        std::size_t w = words_.size();

        while (w >= 8) {
            w -= 8;

            // on newer apple silicon chips this uses the neural engine
            simd::ulong8 group;
            std::memcpy(&group, words_.data() + w, sizeof(group));

            // This instruction finds the max words in a group
            // if max is 0 all 8 are empty
            if (simd_reduce_max(group) == 0) continue;

            // Find the highest nonzero word in this group.
            for (std::size_t lane = 8; lane > 0; --lane) {
                const std::uint64_t word = group[lane - 1];
                if (word != 0)
                    return (w + lane - 1) * 64 + (63 - std::countl_zero(word));
            }
        }

        // Handle fewer than eight remaining words.
        while (w > 0) {
            const std::uint64_t word = words_[--w];
            if (word != 0)
                return w * 64 + (63 - std::countl_zero(word));
        }
        return bit_count_;
    }

private:
    std::size_t bit_count_;
    std::vector<std::uint64_t> words_;
    std::size_t occupied_count = 0;
};

#endif // MATCHINGENGINE_OCCUPANCYBITMAP_H
