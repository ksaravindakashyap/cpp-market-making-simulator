#pragma once

#include "mmsim/event_types.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mmsim {

/// Single-producer single-consumer bounded queue. Capacity must be a power of two >= 2.
/// `head_` is advanced only by the consumer; `tail_` only by the producer.
template <typename T> class SpscRingBuffer {
  public:
    explicit SpscRingBuffer(std::size_t capacity);

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;
    SpscRingBuffer(SpscRingBuffer&&) = delete;
    SpscRingBuffer& operator=(SpscRingBuffer&&) = delete;

    ~SpscRingBuffer() = default;

    [[nodiscard]] bool try_push(const T& value);
    [[nodiscard]] bool try_push(T&& value);

    [[nodiscard]] bool try_pop(T& out);

    /// Number of elements in the queue at some instant (approximate under concurrency).
    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] bool is_empty() const noexcept;

    [[nodiscard]] std::size_t capacity() const noexcept {
        return capacity_;
    }

  private:
    alignas(64) std::atomic<std::uint64_t> head_{0};
    alignas(64) std::atomic<std::uint64_t> tail_{0};

    std::uint64_t capacity_{0};
    std::uint64_t mask_{0};
    std::vector<T> slots_;
};

using EventBus = SpscRingBuffer<Event>;

template <typename T> SpscRingBuffer<T>::SpscRingBuffer(std::size_t capacity) {
    if (capacity < 2) {
        capacity = 2;
    }
    if ((capacity & (capacity - 1)) != 0) {
        std::size_t p = 1;
        while (p < capacity) {
            p <<= 1;
        }
        capacity = p;
    }
    capacity_ = static_cast<std::uint64_t>(capacity);
    mask_ = capacity_ - 1;
    slots_.resize(static_cast<std::size_t>(capacity_));
}

template <typename T> bool SpscRingBuffer<T>::try_push(const T& value) {
    const auto t = tail_.load(std::memory_order_relaxed);
    const auto h = head_.load(std::memory_order_acquire);
    if (t - h >= capacity_) {
        return false;
    }
    slots_[static_cast<std::size_t>(t & mask_)] = value;
    tail_.store(t + 1, std::memory_order_release);
    return true;
}

template <typename T> bool SpscRingBuffer<T>::try_push(T&& value) {
    const auto t = tail_.load(std::memory_order_relaxed);
    const auto h = head_.load(std::memory_order_acquire);
    if (t - h >= capacity_) {
        return false;
    }
    slots_[static_cast<std::size_t>(t & mask_)] = std::move(value);
    tail_.store(t + 1, std::memory_order_release);
    return true;
}

template <typename T> bool SpscRingBuffer<T>::try_pop(T& out) {
    const auto h = head_.load(std::memory_order_relaxed);
    const auto t = tail_.load(std::memory_order_acquire);
    if (h == t) {
        return false;
    }
    out = std::move(slots_[static_cast<std::size_t>(h & mask_)]);
    head_.store(h + 1, std::memory_order_release);
    return true;
}

template <typename T> std::size_t SpscRingBuffer<T>::size() const noexcept {
    const auto h = head_.load(std::memory_order_acquire);
    const auto t = tail_.load(std::memory_order_acquire);
    return static_cast<std::size_t>(t - h);
}

template <typename T> bool SpscRingBuffer<T>::is_empty() const noexcept {
    const auto h = head_.load(std::memory_order_acquire);
    const auto t = tail_.load(std::memory_order_acquire);
    return h == t;
}

} // namespace mmsim
