// Queues: handles, iteration, and reading by handle. Each header a Store here needs is included.

#include "../allocations.hpp"
#include "urd/repr/addressable_heap.hpp"
#include "urd/repr/bucket_queue.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/layer.hpp"
#include "urd/repr/layered_pool.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/queue.hpp"
#include "urd/repr/ring_fifo.hpp"
#include "urd/repr/unordered_flat_map.hpp"
#include "urd/repr/unordered_flat_set.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

using urd::key;
using urd::Store;
using urd::value;

// A layered Store has no ==: its contents include its parent's.
static_assert(!std::equality_comparable<Store<key<int>, urd::layered>> && std::equality_comparable<Store<key<int>>>);

// A priority queue with handles gives the handle from emplace as from push, to update the element later.
TEST(Queues, EmplaceGivesTheHandle)
{
    auto queue = Store<value<int>, urd::by_priority, urd::handles>();
    const auto handle = queue.emplace(5);
    queue.push(3);
    queue.update(handle, 1);
    EXPECT_EQ(queue.top(), 1);
}

// A queue visits its elements: in the order pushed (fifo, lifo), or in no particular order (by priority).
TEST(Queues, IterateTheirElements)
{
    const auto pushed = std::vector { 4, 1, 3, 1 };
    auto fifo = Store<value<int>, urd::fifo>();
    auto ring = Store<value<int>, urd::fifo, urd::retains>();
    auto lifo = Store<value<int>, urd::lifo>();
    auto heap = Store<value<int>, urd::by_priority>();
    auto handled = Store<value<int>, urd::by_priority, urd::handles>();
    for (const auto each : pushed)
    {
        fifo.push(each);
        ring.push(each);
        lifo.push(each);
        heap.push(each);
        handled.push(each);
    }
    fifo.pop();
    ring.pop();
    EXPECT_TRUE(std::ranges::equal(fifo, std::vector { 1, 3, 1 }));
    EXPECT_TRUE(std::ranges::equal(ring, std::vector { 1, 3, 1 }));
    EXPECT_EQ(fifo.back(), 1);
    EXPECT_EQ(ring.back(), 1);
    EXPECT_TRUE(std::ranges::equal(lifo, pushed));
    const auto sorted = [](const auto& queue)
    {
        auto elements = std::vector<int>(queue.begin(), queue.end());
        std::ranges::sort(elements);
        return elements;
    };
    EXPECT_EQ(sorted(heap), (std::vector { 1, 1, 3, 4 }));
    EXPECT_EQ(sorted(handled), (std::vector { 1, 1, 3, 4 }));
}
template<typename S>
concept iterates = requires(const S& store) { store.begin(); };
static_assert(iterates<Store<value<int>, urd::fifo>> && !iterates<Store<value<int>, urd::by_priority, urd::monotone_priority>>);

// A priority queue with handles reads an element by its handle, wherever it is in the order.
TEST(Queues, ReadByHandle)
{
    auto queue = Store<value<int>, urd::by_priority, urd::handles>();
    const auto five = queue.push(5);
    queue.push(1);
    EXPECT_EQ(queue[five], 5);
    queue.update(five, 0);
    EXPECT_EQ(queue[five], 0);
}

}  // namespace
