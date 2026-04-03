/**
 * Standalone manual tests for mmsim::SpscRingBuffer (lock-free SPSC ring).
 * Build: test_event_bus_manual (see tests/CMakeLists.txt)
 */

#include "mmsim/event_bus.h"

#include <barrier>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

using mmsim::SpscRingBuffer;

int g_failures = 0;

void fail(const char* msg) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_failures;
}

void pass(const char* msg) {
    std::printf("PASS: %s\n", msg);
}

// --- 1. Basic FIFO: 100 push, 100 pop ---
bool test_basic_fifo() {
    SpscRingBuffer<std::uint64_t> rb(128);
    for (std::uint64_t i = 0; i < 100; ++i) {
        if (!rb.try_push(i)) {
            fail("basic: unexpected try_push failure");
            return false;
        }
    }
    for (std::uint64_t i = 0; i < 100; ++i) {
        std::uint64_t v = 0;
        if (!rb.try_pop(v)) {
            fail("basic: unexpected try_pop failure");
            return false;
        }
        if (v != i) {
            fail("basic: FIFO mismatch");
            return false;
        }
    }
    if (!rb.is_empty()) {
        fail("basic: not empty after drain");
        return false;
    }
    return true;
}

// --- 2. Full buffer: try_push false when full ---
bool test_full() {
    constexpr std::size_t cap = 64;
    SpscRingBuffer<int> rb(cap);
    for (int i = 0; i < static_cast<int>(cap); ++i) {
        if (!rb.try_push(i)) {
            fail("full: push failed before full");
            return false;
        }
    }
    if (rb.try_push(-1)) {
        fail("full: try_push should fail when full");
        return false;
    }
    if (rb.size() != cap) {
        fail("full: size at capacity");
        return false;
    }
    int x = 0;
    for (int i = 0; i < static_cast<int>(cap); ++i) {
        if (!rb.try_pop(x) || x != i) {
            fail("full: pop after full");
            return false;
        }
    }
    if (!rb.is_empty()) {
        fail("full: empty after drain");
        return false;
    }
    return true;
}

// --- 3. Empty: try_pop false ---
bool test_empty_pop() {
    SpscRingBuffer<double> rb(8);
    double x = 0.0;
    if (rb.try_pop(x)) {
        fail("empty: try_pop should fail");
        return false;
    }
    if (!rb.try_push(3.14)) {
        fail("empty: push after empty test");
        return false;
    }
    if (!rb.try_pop(x) || x != 3.14) {
        fail("empty: single round-trip");
        return false;
    }
    if (rb.try_pop(x)) {
        fail("empty: second pop should fail");
        return false;
    }
    return true;
}

// --- 4. Wraparound: interleaved push/pop, many cycles (capacity 16) ---
bool test_wraparound() {
    constexpr std::size_t cap = 16;
    constexpr std::uint64_t total = 10000;
    SpscRingBuffer<std::uint64_t> rb(cap);

    std::uint64_t next_push = 0;
    std::uint64_t next_expect = 0;

    while (next_push < total) {
        if (rb.try_push(next_push)) {
            ++next_push;
        } else {
            std::uint64_t v = 0;
            if (!rb.try_pop(v)) {
                fail("wrap: pop failed when push failed");
                return false;
            }
            if (v != next_expect) {
                fail("wrap: sequence corruption");
                return false;
            }
            ++next_expect;
        }
    }

    std::uint64_t v = 0;
    while (next_expect < total) {
        if (!rb.try_pop(v)) {
            fail("wrap: final drain pop");
            return false;
        }
        if (v != next_expect) {
            fail("wrap: final sequence");
            return false;
        }
        ++next_expect;
    }

    if (!rb.is_empty()) {
        fail("wrap: not empty");
        return false;
    }

    // At least 3 full index wraps: total pushes >> cap
    const std::uint64_t min_wrap_events = 3 * cap;
    if (total < min_wrap_events) {
        fail("wrap: test length");
        return false;
    }
    return true;
}

// --- 5 & 6. Concurrent 1M + throughput ---
bool test_concurrent_million_and_throughput(double* out_events_per_sec) {
    constexpr std::uint64_t k_items = 1'000'000;
    SpscRingBuffer<std::uint64_t> rb(4096);
    std::barrier start(2);
    std::atomic<bool> order_ok{true};

    const auto t0 = std::chrono::steady_clock::now();

    std::thread producer([&] {
        start.arrive_and_wait();
        for (std::uint64_t i = 0; i < k_items; ++i) {
            while (!rb.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        start.arrive_and_wait();
        for (std::uint64_t i = 0; i < k_items; ++i) {
            std::uint64_t v = 0;
            while (!rb.try_pop(v)) {
                std::this_thread::yield();
            }
            if (v != i) {
                order_ok.store(false);
                return;
            }
        }
    });

    producer.join();
    consumer.join();

    const auto t1 = std::chrono::steady_clock::now();

    if (!order_ok.load()) {
        fail("concurrent: order mismatch");
        return false;
    }
    const double sec = std::chrono::duration<double>(t1 - t0).count();
    *out_events_per_sec = static_cast<double>(k_items) / sec;

    if (!rb.is_empty()) {
        fail("concurrent: buffer not empty");
        return false;
    }

    std::printf("Throughput (concurrent 1M): %.0f events/sec (wall %.3f s)\n", *out_events_per_sec, sec);

    if (*out_events_per_sec < 1'000'000.0) {
        std::fprintf(stderr, "WARN: throughput below 1M events/sec (minimum per spec)\n");
    }

    return true;
}

} // namespace

int main() {
    std::printf("=== test_event_bus_manual (SpscRingBuffer) ===\n\n");

    double eps = 0.0;

    if (test_basic_fifo()) {
        pass("1. Basic FIFO (100 events)");
    } else {
        fail("1. Basic FIFO");
    }

    if (test_full()) {
        pass("2. Full buffer (try_push false at capacity)");
    } else {
        fail("2. Full buffer");
    }

    if (test_empty_pop()) {
        pass("3. Empty buffer (try_pop false)");
    } else {
        fail("3. Empty buffer");
    }

    if (test_wraparound()) {
        pass("4. Wraparound (10k events, cap 16, many index wraps)");
    } else {
        fail("4. Wraparound");
    }

    if (test_concurrent_million_and_throughput(&eps)) {
        pass("5–6. Concurrent 1M + throughput (see line above)");
    } else {
        fail("5–6. Concurrent 1M + throughput");
    }

    std::printf("\n--- Summary: %d failure(s) ---\n", g_failures);
    if (eps > 0.0) {
        std::printf("Measured throughput: %.0f events/sec\n", eps);
    }

    return g_failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
