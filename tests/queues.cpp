// Queues: elements reached by the order they leave in. Each container declaring an order of leaving (fifo, lifo,
// by_priority) is checked against a model through random pushes and pops, as tests/properties.cpp checks sets and
// sequences.

#include "allocations.hpp"
#include "urd/store.hpp"
#include "verify.hpp"

#include <concepts>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

using urd::Store;
using urd::value;

namespace
{

template<typename Q>
class Queues : public testing::Test
{
};
using Containers = testing::Types<urd::repr::Fifo<int>,
                                  urd::repr::Lifo<int>,
                                  urd::repr::Heap<int>,
                                  Store<value<int>, urd::fifo>,
                                  Store<value<int>, urd::lifo>,
                                  Store<value<int>, urd::by_priority>,
                                  Store<value<int>, urd::lifo, urd::retains>>;
TYPED_TEST_SUITE(Queues, Containers);

}  // namespace

// What each request resolves to: a queue for its order.
static_assert(std::same_as<Store<value<int>, urd::fifo>::representation, urd::repr::Fifo<int>>);
static_assert(std::same_as<Store<value<int>, urd::lifo>::representation, urd::repr::Lifo<int>>);
static_assert(std::same_as<Store<value<int>, urd::by_priority>::representation, urd::repr::Heap<int>>);
// Reused per search, it keeps its storage; a monotone one never clears.
template<typename Q>
concept clearable = requires(Q queue) { queue.clear(); };
static_assert(clearable<Store<value<int>, urd::by_priority>> && !clearable<Store<value<int>, urd::by_priority, urd::monotone>>);
static_assert(urd::has<Store<value<int>, urd::by_priority, urd::retains>, urd::retains>);
// A queue is not iterated or indexed; it offers the adaptors' names for its order.
template<typename Q>
concept iterable = requires(Q queue) { queue.begin(); };
template<typename Q>
concept has_front = requires(Q queue) { queue.front(); };
template<typename Q>
concept has_top = requires(Q queue) { queue.top(); };
static_assert(!iterable<Store<value<int>, urd::fifo>> && has_front<Store<value<int>, urd::fifo>> && !has_top<Store<value<int>, urd::fifo>>);
static_assert(has_top<Store<value<int>, urd::by_priority>> && !has_front<Store<value<int>, urd::by_priority>>);

TYPED_TEST(Queues, OffersWhatItsPropertiesRequire) { static_assert(urd::conforming<TypeParam>); }

TYPED_TEST(Queues, LeavesInItsOrderThroughRandomOperations)
{
    for (unsigned seed = 0; seed < 64; ++seed)
    {
        const auto found = urd::verify(TypeParam(), seed, 1000);
        EXPECT_FALSE(found) << found.value_or("");
    }
}

// A queue that retains its storage refills after clear, or after popping everything, without allocating.
TYPED_TEST(Queues, Retains)
{
    if (!urd::test::counts_allocations)
        GTEST_SKIP() << "allocations are not counted under ThreadSanitizer";
    if constexpr (!urd::has<TypeParam, urd::retains>)
        GTEST_SKIP() << "not declared";
    else
    {
        constexpr int count = 1000;
        auto queue = TypeParam();
        for (int element = 0; element < count; ++element)
            queue.push(element);
        queue.clear();
        {
            const auto counted = urd::test::Counted();
            for (int element = 0; element < count; ++element)
                queue.push(element);
        }
        EXPECT_EQ(urd::test::allocations(), 0) << "after clear";
        while (!queue.empty())
            queue.pop();
        {
            const auto counted = urd::test::Counted();
            for (int element = 0; element < count; ++element)
                queue.push(element);
        }
        EXPECT_EQ(urd::test::allocations(), 0) << "after popping every element";
    }
}

// A search's open list: the cheapest node first.
TEST(Queues, PopsTheCheapestFirst)
{
    auto open = Store<value<std::pair<double, int>>, urd::by_priority>();
    open.push({ 3.0, 1 });
    open.push({ 1.5, 2 });
    open.emplace(2.0, 3);
    auto order = std::vector<int>();
    while (!open.empty())
    {
        order.push_back(open.top().second);
        open.pop();
    }
    EXPECT_EQ(order, (std::vector<int> { 2, 3, 1 }));
}
