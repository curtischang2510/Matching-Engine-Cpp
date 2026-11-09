#ifndef MATCHING_ENGINE_CPP_SPSC_BUFFER_V1_H
#define MATCHING_ENGINE_CPP_SPSC_BUFFER_V1_H

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

// Baseline: both indices use normal alignment.
template <typename T>
class SPSCBufferV1 {
    std::vector<T> buffer;
    const std::size_t capacity;
    std::atomic<std::size_t> head{0};
    std::atomic<std::size_t> tail{0};

    std::size_t nextIndex(std::size_t index) const {
        ++index;
        return index == capacity ? 0 : index;
    }

public:
    explicit SPSCBufferV1(std::size_t capacity) : capacity(capacity) {
        if (capacity < 2) throw std::invalid_argument("capacity must be at least 2");
        buffer.resize(capacity);
    }

    bool try_push(const T& element) {
        const std::size_t t = tail.load(std::memory_order_relaxed);
        const std::size_t next_t = nextIndex(t);
        const std::size_t h = head.load(std::memory_order_acquire);
        if (next_t == h) return false;

        buffer[t] = element;
        tail.store(next_t, std::memory_order_release);
        return true;
    }

    bool try_pop(T& result) {
        const std::size_t h = head.load(std::memory_order_relaxed);
        const std::size_t t = tail.load(std::memory_order_acquire);
        if (t == h) return false;

        result = std::move(buffer[h]);
        head.store(nextIndex(h), std::memory_order_release);
        return true;
    }
};

#endif
