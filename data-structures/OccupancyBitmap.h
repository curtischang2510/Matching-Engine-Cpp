#ifndef MATCHINGENGINE_OCCUPANCYBITMAP_H
#define MATCHINGENGINE_OCCUPANCYBITMAP_H

#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

class OccupancyBitmap {
public:
    explicit OccupancyBitmap(std::size_t bit_count)
        : bit_count_(bit_count), words_((bit_count + 63) / 64, 0) {}

    void set(std::size_t i) {
        std::uint64_t& word = words_[i / 64];
        const std::uint64_t mask = std::uint64_t{1} << (i % 64);
        if ((word & mask) == 0) {
            word |= mask;
            ++occupied_count;
        }
    }

    void clear(std::size_t i) {
        std::uint64_t& word = words_[i / 64];
        const std::uint64_t mask = std::uint64_t{1} << (i % 64);
        if (word & mask) {
            word &= ~mask;
            --occupied_count;
        }
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

private:
    std::size_t bit_count_;
    std::vector<std::uint64_t> words_;
    std::size_t occupied_count = 0;
};

#endif // MATCHINGENGINE_OCCUPANCYBITMAP_H
