// Maps whose values merge, nested dense maps, and maps that recycle their nodes. Each header a Store here needs is included.

#include "../allocations.hpp"
#include "urd/compose/family.hpp"
#include "urd/concepts.hpp"
#include "urd/repr/bidirectional_csr.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/csr_multimap.hpp"
#include "urd/repr/dense_multimap.hpp"
#include "urd/repr/dense_set.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/index_map.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/parallel_hash.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/recycling.hpp"
#include "urd/repr/scoped.hpp"
#include "urd/repr/sparse_set.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
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
template<typename K, typename V>
using TreeMapOf = std::map<K, V, urd::Less<K>>;

// A two-level dense map is two dense Stores nested: cleared at the outer level in O(1), refilled fresh.
TEST(Nested, TwoLevelDenseMap)
{
    using Rows = Store<key<int>, value<double>, urd::dense>;
    auto relations = Store<key<int>, value<Rows>, urd::dense, urd::constant_clear>();
    relations[2][5] = 1.5;
    relations[2][7] = 2.5;
    relations[0][1] = 0.5;
    EXPECT_EQ(relations.at(2).size(), 2U);
    relations.clear();
    EXPECT_TRUE(relations.empty());
    relations[2][1] = 3.0;
    EXPECT_EQ(relations.at(2).size(), 1U);
    EXPECT_FALSE(relations.at(2).contains(5));
}

namespace
{

/// A closed interval, merged by hull.
struct Interval
{
    double lower = 0;
    double upper = 0;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Interval::lower, &Interval::upper> {}; }
};
constexpr auto hull = [](const Interval& lhs, const Interval& rhs) { return Interval { std::min(lhs.lower, rhs.lower), std::max(lhs.upper, rhs.upper) }; };

template<typename S>
class Merge : public testing::Test
{
};
using MergeMaps = testing::Types<Store<key<int>, value<int>>,
                                 Store<key<int>, value<int>, urd::sorted>,
                                 Store<key<int>, value<int>, urd::dense>,
                                 Store<key<int>, value<int>, urd::dense, urd::constant_clear>,
                                 Store<key<int>, value<int>, urd::insertion_ordered>,
                                 Store<key<int>, value<int>, urd::dense, urd::insertion_ordered>,
                                 Store<key<int>, value<int>, urd::scoped>>;
TYPED_TEST_SUITE(Merge, MergeMaps);

}  // namespace

// merge inserts where a key has no value, combines where it has one, and says whether the map changed: against a
// std::map merged by hand.
TYPED_TEST(Merge, CombinesAndReportsChange)
{
    auto random = std::mt19937(41);
    auto map = TypeParam();
    auto expected = std::map<int, int>();
    const auto lower = [](int lhs, int rhs) { return std::min(lhs, rhs); };
    for (int step = 0; step < 2000; ++step)
    {
        const int key = static_cast<int>(random() % 30);
        const int value = static_cast<int>(random() % 100);
        const auto [found, inserted] = expected.try_emplace(key, value);
        const bool changed = inserted || value < found->second;
        found->second = std::min(found->second, value);
        ASSERT_EQ(map.merge(key, value, lower), changed) << "step " << step;
        ASSERT_EQ(map.at(key), expected.at(key));
    }
    EXPECT_EQ(map.size(), expected.size());
}

// An interval widened by hull only where it grows.
TEST(Merge, HullOfIntervals)
{
    auto values = Store<key<int>, value<Interval>, urd::dense, urd::insertion_ordered>();
    EXPECT_TRUE(values.merge(3, Interval { 1, 2 }, hull));
    EXPECT_FALSE(values.merge(3, Interval { 1.5, 2 }, hull));
    EXPECT_TRUE(values.merge(3, Interval { 0, 1 }, hull));
    EXPECT_EQ(values.at(3).lower, 0);
    EXPECT_EQ(values.at(3).upper, 2);
}

// update hands each value to a function with whether the key had one; read reads it or gives a default. Against a
// std::map updated by hand, over the map kinds of the merge suite.
TYPED_TEST(Merge, UpdateAndRead)
{
    auto random = std::mt19937(47);
    auto map = TypeParam();
    auto expected = std::map<int, int>();
    for (int step = 0; step < 2000; ++step)
    {
        const int key = static_cast<int>(random() % 30);
        const bool had = expected.contains(key);
        const auto reported = map.update(key,
                                         [](int& count, bool held)
                                         {
                                             count = held ? count + 1 : 1;
                                             return held;
                                         });
        ASSERT_EQ(reported, had) << "step " << step;
        expected[key] = had ? expected[key] + 1 : 1;
        const int probe = static_cast<int>(random() % 32);
        ASSERT_EQ(map.read(probe, [](int count) { return count; }, -1), expected.contains(probe) ? expected[probe] : -1);
    }
}

// A map cleared by generation hands over the value it kept, storage and all: a buffer refilled per round allocates
// once.
TEST(Merge, GenerationsReuseValues)
{
    auto buffers = Store<key<int>, value<std::vector<int>>, urd::dense, urd::constant_clear>();
    buffers.update(3, [](std::vector<int>& buffer, bool) { buffer.assign(1000, 7); });
    buffers.clear();
    EXPECT_FALSE(buffers.contains(3));
    buffers.update(3,
                   [](std::vector<int>& buffer, bool held)
                   {
                       EXPECT_FALSE(held);
                       EXPECT_GE(buffer.capacity(), 1000U);
                       buffer.clear();
                   });
    EXPECT_TRUE(buffers.contains(3));
    EXPECT_TRUE(buffers.at(3).empty());
}

// Threads updating and reading the same keys of a thread-safe generation map at once: every update counts.
TEST(Merge, ThreadsUpdateTogether)
{
    auto counts = Store<key<int>, value<int>, urd::dense, urd::constant_clear, urd::concurrent>();
    static_assert(std::same_as<decltype(counts)::representation, urd::repr::IndexMap<int, int, true, true>>);
    constexpr int threads = 4;
    constexpr int rounds = 20000;
    constexpr int keys = 50;
    {
        auto workers = std::vector<std::jthread>();
        for (int thread = 0; thread < threads; ++thread)
            workers.emplace_back(
                [&counts, thread]
                {
                    for (int round = 0; round < rounds; ++round)
                    {
                        counts.update((round + thread) % keys, [](int& count, bool held) { count = held ? count + 1 : 1; });
                        (void) counts.read(round % keys, [](int count) { return count; }, 0);
                    }
                });
    }
    int total = 0;
    for (int key = 0; key < keys; ++key)
        total += counts.at(key);
    EXPECT_EQ(total, threads * rounds);
}

// Threads updating and reading the same hashed keys of a shared map at once: every update counts.
TEST(Merge, ThreadsUpdateHashedKeysTogether)
{
    auto counts = Store<key<std::string>, value<int>, urd::concurrent>();
    constexpr int threads = 4;
    constexpr int rounds = 20000;
    constexpr int keys = 50;
    {
        auto workers = std::vector<std::jthread>();
        for (int thread = 0; thread < threads; ++thread)
            workers.emplace_back(
                [&counts, thread]
                {
                    for (int round = 0; round < rounds; ++round)
                    {
                        counts.update(std::to_string((round + thread) % keys), [](int& count, bool held) { count = held ? count + 1 : 1; });
                        (void) counts.read(std::to_string(round % keys), [](int count) { return count; }, 0);
                    }
                });
    }
    int total = 0;
    for (int key = 0; key < keys; ++key)
        total += counts.at(std::to_string(key));
    EXPECT_EQ(total, threads * rounds);
}

// A sorted map whose values are rows that keep their memory: erased keys leave their nodes and values for the next
// keys, so buckets of a cost queue, emptied and refilled, allocate nothing once warm.
TEST(Recycling, BucketsRefillWithoutAllocating)
{
    using Bucket = Store<key<int>, urd::retains>;
    using Buckets = Store<key<double>, value<Bucket>, urd::sorted>;
    static_assert(std::same_as<Buckets::representation, urd::repr::RecyclingMap<double, Bucket>>);
    // Values that may free their memory stay in a plain tree.
    static_assert(std::same_as<Store<key<double>, value<Store<key<int>>>, urd::sorted>::representation, TreeMapOf<double, Store<key<int>>>>);
    auto buckets = Buckets();
    const auto fill = [&buckets](double cost)
    {
        for (int each = 0; each < 100; ++each)
            buckets[cost].insert(each + static_cast<int>(cost * 1000));
    };
    for (int round = 0; round < 3; ++round)
    {
        fill(1.0 + round);
        fill(2.0 + round);
        buckets.erase(buckets.begin()->first);
        buckets.erase(buckets.begin()->first);
    }
    {
        const auto counted = urd::test::Counted();
        fill(7.5);
        fill(8.5);
    }
    if (urd::test::counts_allocations)
    {
        EXPECT_EQ(urd::test::allocations(), 0);
    }
    EXPECT_EQ(buckets.size(), 2U);
    EXPECT_EQ(buckets.begin()->first, 7.5);
    EXPECT_EQ(buckets.at(8.5).size(), 100U);
}

// Without an order asked for, values that keep their memory get a hash map that recycles: no tree where none is needed.
// Sets with stable references that keep their storage: the node sets, their erased nodes kept for the next elements.
static_assert(std::same_as<Store<key<int>, urd::stable_refs, urd::retains>::representation, urd::repr::RecyclingHashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::stable_refs, urd::retains>::representation, urd::repr::RecyclingTreeSet<int>>);
// Without stable_refs, retains is the flat containers' already.
static_assert(std::same_as<Store<key<int>, urd::retains>::representation, boost::unordered_flat_set<int, urd::Hash<int>, urd::EqualTo<int>>>);

TEST(Recycling, UnorderedMapsRecycleToo)
{
    using Bucket = Store<key<int>, urd::retains>;
    using Groups = Store<key<int>, value<Bucket>>;
    static_assert(std::same_as<Groups::representation, urd::repr::RecyclingHashMap<int, Bucket>>);
    auto groups = Groups();
    for (int round = 0; round < 3; ++round)
    {
        for (int each = 0; each < 50; ++each)
            groups[round].insert(each + (round * 100));
        groups.erase(round);
    }
    {
        const auto counted = urd::test::Counted();
        for (int each = 0; each < 50; ++each)
            groups[7].insert(each + 700);
    }
    if (urd::test::counts_allocations)
    {
        EXPECT_EQ(urd::test::allocations(), 0);
    }
    EXPECT_EQ(groups.at(7).size(), 50U);
    EXPECT_FALSE(groups.at(7).contains(0));
}

}  // namespace
