// Queues: elements reached by the order they leave in. Each container declaring an order of leaving (fifo, lifo,
// by_priority) is checked against a model through random pushes and pops, as tests/properties.cpp checks sets and
// sequences.

#include "allocations.hpp"
#include "urd/store.hpp"
#include "verify.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <gtest/gtest.h>
#include <iterator>
#include <map>
#include <memory>
#include <random>
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
                                  urd::repr::AddressableHeap<int>,
                                  urd::repr::BucketQueue<int>,
                                  Store<value<int>, urd::by_priority, urd::handles>,
                                  Store<value<int>, urd::by_priority, urd::monotone_priority>,
                                  Store<value<int>, urd::fifo>,
                                  Store<value<int>, urd::fifo, urd::stable_refs>,
                                  Store<value<int>, urd::lifo>,
                                  Store<value<int>, urd::by_priority>,
                                  Store<value<int>, urd::lifo, urd::retains>>;
TYPED_TEST_SUITE(Queues, Containers);

}  // namespace

// What each request resolves to: a queue for its order.
static_assert(std::same_as<Store<value<int>, urd::fifo>::representation, urd::repr::Fifo<int>>);
// A deque keeps the front in place while elements queue behind it.
static_assert(std::same_as<Store<value<int>, urd::fifo, urd::stable_refs>::representation, urd::repr::Fifo<int>>);
static_assert(std::same_as<Store<value<int>, urd::lifo>::representation, urd::repr::Lifo<int>>);
static_assert(std::same_as<Store<value<int>, urd::by_priority>::representation, urd::repr::Heap<int>>);
// Handles to change or erase an element; integral priorities pushed in order fill buckets; others stay a heap.
static_assert(std::same_as<Store<value<int>, urd::by_priority, urd::handles>::representation, urd::repr::AddressableHeap<int>>);
static_assert(std::same_as<Store<value<int>, urd::by_priority, urd::monotone_priority>::representation, urd::repr::BucketQueue<int>>);
// A monotone queue keeps pop: pop is how a queue is consumed, monotone only takes clear away.
static_assert(requires(Store<value<int>, urd::by_priority, urd::monotone> queue) { queue.pop(); });
static_assert(std::same_as<Store<value<double>, urd::by_priority, urd::monotone_priority>::representation, urd::repr::Heap<double>>);
static_assert(std::same_as<
              Store<value<std::pair<unsigned, int>>, urd::by_priority, urd::monotone_priority, urd::order_by<&std::pair<unsigned, int>::first>>::representation,
              urd::repr::BucketQueue<std::pair<unsigned, int>, urd::ProjectedLess<std::pair<unsigned, int>, &std::pair<unsigned, int>::first>>>);
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

// A fifo with stable references: the front stays put while elements queue behind it, and so does the element that
// becomes the front after a pop.
TYPED_TEST(Queues, StableRefs)
{
    if constexpr (!urd::has<TypeParam, urd::stable_refs> || !has_front<TypeParam>)
        GTEST_SKIP() << "not declared, or no front";
    else
    {
        auto queue = TypeParam();
        queue.push(0);
        queue.push(1);
        const auto* front = std::addressof(std::as_const(queue).front());
        for (int element = 2; element < 100000; ++element)
            queue.push(element);
        EXPECT_EQ(std::addressof(std::as_const(queue).front()), front);
        queue.pop();
        const auto* next = std::addressof(std::as_const(queue).front());
        for (int element = 0; element < 100000; ++element)
            queue.push(element);
        EXPECT_EQ(std::addressof(std::as_const(queue).front()), next);
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

// An addressable heap changed and erased by handle at random, against a model of the handles' elements. Each value is
// unique (a random part, then the step), so the model knows which handle a pop removes.
TEST(Queues, AddressableHeapFollowsUpdatesAndErases)
{
    for (unsigned seed = 0; seed < 64; ++seed)
    {
        auto random = std::mt19937(seed);
        const auto pick = [&random](int bound) { return std::uniform_int_distribution<int>(0, bound - 1)(random); };
        constexpr int steps = 1000;
        auto heap = Store<value<int>, urd::by_priority, urd::handles>();
        auto model = std::map<std::size_t, int>();
        for (int step = 0; step < steps; ++step)
        {
            const auto unique = [&pick, step] { return (pick(100) * steps) + step; };
            const auto action = pick(4);
            if (action == 0 || model.empty())
            {
                const auto element = unique();
                model[heap.push(element)] = element;
            }
            else
            {
                auto held = model.begin();
                std::advance(held, pick(static_cast<int>(model.size())));
                if (action == 1)
                {
                    held->second = unique();
                    heap.update(held->first, held->second);
                }
                else if (action == 2)
                {
                    heap.erase(held->first);
                    model.erase(held);
                }
                else
                {
                    const auto least = std::ranges::min_element(model, {}, [](const auto& entry) { return entry.second; });
                    ASSERT_EQ(heap.top(), least->second) << "seed " << seed << " step " << step;
                    heap.pop();
                    model.erase(least);
                }
            }
            ASSERT_EQ(heap.size(), model.size()) << "seed " << seed << " step " << step;
            if (!model.empty())
            {
                ASSERT_EQ(heap.top(), std::ranges::min_element(model, {}, [](const auto& entry) { return entry.second; })->second);
            }
        }
    }
}
