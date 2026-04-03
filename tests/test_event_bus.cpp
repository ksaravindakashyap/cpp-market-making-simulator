#include "mmsim/event_bus.h"

#include <barrier>
#include <cstdint>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace {

TEST(SpscRingBuffer, TryPushTryPopSingleThread) {
    mmsim::SpscRingBuffer<int> rb(8);
    EXPECT_TRUE(rb.is_empty());
    EXPECT_EQ(rb.size(), 0u);

    EXPECT_TRUE(rb.try_push(42));
    EXPECT_FALSE(rb.is_empty());
    EXPECT_EQ(rb.size(), 1u);

    int v = 0;
    EXPECT_TRUE(rb.try_pop(v));
    EXPECT_EQ(v, 42);
    EXPECT_TRUE(rb.is_empty());
}

TEST(SpscRingBuffer, FullReturnsFalse) {
    mmsim::SpscRingBuffer<int> rb(4);
    EXPECT_TRUE(rb.try_push(1));
    EXPECT_TRUE(rb.try_push(2));
    EXPECT_TRUE(rb.try_push(3));
    EXPECT_TRUE(rb.try_push(4));
    EXPECT_FALSE(rb.try_push(5));

    int x = 0;
    EXPECT_TRUE(rb.try_pop(x));
    EXPECT_EQ(x, 1);
    EXPECT_TRUE(rb.try_push(5));
}

TEST(SpscRingBuffer, RoundsUpToPowerOfTwo) {
    mmsim::SpscRingBuffer<int> rb(100);
    EXPECT_EQ(rb.capacity(), 128u);
}

TEST(SpscRingBuffer, ConcurrentProducerConsumerPreservesOrder) {
    constexpr int kItems = 200'000;
    mmsim::SpscRingBuffer<int> rb(1024);
    std::barrier start(2);

    std::thread producer([&] {
        start.arrive_and_wait();
        for (int i = 0; i < kItems; ++i) {
            while (!rb.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        start.arrive_and_wait();
        for (int i = 0; i < kItems; ++i) {
            int v = -1;
            while (!rb.try_pop(v)) {
                std::this_thread::yield();
            }
            ASSERT_EQ(v, i) << "Mismatch at index " << i;
        }
    });

    producer.join();
    consumer.join();
    EXPECT_TRUE(rb.is_empty());
}

TEST(SpscRingBuffer, ConcurrentStressNoLoss) {
    constexpr std::uint64_t kItems = 500'000;
    mmsim::SpscRingBuffer<std::uint64_t> rb(2048);
    std::barrier start(2);

    std::thread producer([&] {
        start.arrive_and_wait();
        for (std::uint64_t i = 0; i < kItems; ++i) {
            while (!rb.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::uint64_t sum = 0;
    std::thread consumer([&] {
        start.arrive_and_wait();
        for (std::uint64_t n = 0; n < kItems; ++n) {
            std::uint64_t v = 0;
            while (!rb.try_pop(v)) {
                std::this_thread::yield();
            }
            sum += v;
        }
    });

    producer.join();
    consumer.join();

    const std::uint64_t expected = (kItems - 1) * kItems / 2;
    EXPECT_EQ(sum, expected);
    EXPECT_TRUE(rb.is_empty());
}

TEST(SpscRingBuffer, ConcurrentWithTightBuffer) {
    constexpr int kItems = 50'000;
    mmsim::SpscRingBuffer<int> rb(16);
    std::barrier start(2);

    std::thread producer([&] {
        start.arrive_and_wait();
        for (int i = 0; i < kItems; ++i) {
            while (!rb.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        start.arrive_and_wait();
        for (int i = 0; i < kItems; ++i) {
            int v = -1;
            while (!rb.try_pop(v)) {
                std::this_thread::yield();
            }
            ASSERT_EQ(v, i);
        }
    });

    producer.join();
    consumer.join();
}

TEST(EventBus, EventRoundTrip) {
    mmsim::EventBus bus(32);
    mmsim::Event e{};
    e.type = mmsim::EventType::OrderNew;
    e.order_new.order_id = 99;
    e.order_new.side = mmsim::Side::BUY;
    e.order_new.price = 10000;
    e.order_new.quantity = 5;

    ASSERT_TRUE(bus.try_push(e));
    mmsim::Event out{};
    ASSERT_TRUE(bus.try_pop(out));
    EXPECT_EQ(out.type, mmsim::EventType::OrderNew);
    EXPECT_EQ(out.order_new.order_id, 99u);
    EXPECT_EQ(out.order_new.side, mmsim::Side::BUY);
    EXPECT_EQ(out.order_new.price, 10000);
    EXPECT_EQ(out.order_new.quantity, 5);
}

} // namespace
