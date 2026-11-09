//
// Created by Curtis on 30/10/26.
//

#ifndef MATCHING_ENGINE_CPP_SPSC_BUFFER_V3_H
#define MATCHING_ENGINE_CPP_SPSC_BUFFER_V3_H
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename T>
class SPSCBufferV3
{
private:
    std::vector<T> buffer;
    const std::size_t capacity;
    std::atomic<std::size_t> head{0};
    std::atomic<std::size_t> tail{0};
    // Each cached index belongs to one thread; refresh it only when needed.
    std::size_t cached_head_{0};
    std::size_t cached_tail_{0};

    // consider adding no discard
    std::size_t nextIndex(std::size_t index) const {
        ++index;
        return index == capacity ? 0 : index;
    }

public:
    explicit SPSCBufferV3(std::size_t capacity): capacity(capacity) {
        if (capacity < 2) {
            throw std::invalid_argument("capacity must be at least 2");
        }
        buffer.resize(capacity);
    }

    // Push to the tail
    bool try_push(const T& element) {
        const std::size_t t = tail.load(std::memory_order_relaxed);
        const std::size_t next_t = nextIndex(t);

        // the cached value says buffer is full but possible value is outdated
        // note this works because the other thread can only "increase" the head
        if (next_t == cached_head_) {
            cached_head_ = head.load(std::memory_order_acquire);
            if (next_t == cached_head_) { // actually full
                return false;
            }
        }

        buffer[t] = element; // copy
        tail.store(next_t, std::memory_order_release);
        return true;
    }

    // Pop from the head
    bool try_pop(T& result) {
        const std::size_t h = head.load(std::memory_order_relaxed);

        if (h == cached_tail_) {
            cached_tail_ = tail.load(std::memory_order_acquire);
            if (h == cached_tail_) { // actually empty
                return false;
            }
        }

        result = std::move(buffer[h]);
        const std::size_t next_h = nextIndex(h);
        head.store(next_h, std::memory_order_release);
        return true;
    }
};

#endif // MATCHING_ENGINE_CPP_SPSC_BUFFER_V3_H
