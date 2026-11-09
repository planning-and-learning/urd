// Maps whose values merge, nested dense maps, and maps that recycle their nodes. Each header a Store here needs is included.

#include "urd/testing/allocations.hpp"
#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/parallel_hash_map.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/pools/pools.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/parallel_hash_set.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/values/std_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/repr/wrappers/layer.hpp"
#include "urd/repr/wrappers/layered_pool.hpp"
#include "urd/repr/wrappers/recycling.hpp"
#include "urd/repr/wrappers/scoped.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <array>
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
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

using urd::map;
using urd::set;
using urd::Store;
template<typename K, typename V>
using TreeMapOf = std::map<K, V, urd::Less<K>>;

// A two-level dense map is two dense Stores nested: cleared at the outer level in O(1), refilled fresh.
TEST(Nested, TwoLevelDenseMap)
{
    using Rows = Store<map<int, double>, urd::dense>;
    auto relations = Store<map<int, Rows>, urd::dense, urd::constant_clear>();
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
using MergeMaps = testing::Types<Store<map<int, int>>,
                                 Store<map<int, int>, urd::sorted>,
                                 Store<map<int, int>, urd::dense>,
                                 Store<map<int, int>, urd::dense, urd::constant_clear>,
                                 Store<map<int, int>, urd::insertion_ordered>,
                                 Store<map<int, int>, urd::dense, urd::insertion_ordered>,
                                 Store<map<int, int>, urd::scoped>>;
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
    auto values = Store<map<int, Interval>, urd::dense, urd::insertion_ordered>();
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
    auto buffers = Store<map<int, std::vector<int>>, urd::dense, urd::constant_clear>();
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

// A sorted map whose values are rows that keep their memory: erased keys leave their nodes and values for the next
// keys, so buckets of a cost queue, emptied and refilled, allocate nothing once warm.
TEST(Recycling, BucketsRefillWithoutAllocating)
{
    using Bucket = Store<set<int>, urd::retains>;
    using Buckets = Store<map<double, Bucket>, urd::sorted>;
    static_assert(std::same_as<Buckets::representation, urd::repr::RecyclingMap<double, Bucket>>);
    // Values that may free their memory stay in a plain tree.
    static_assert(std::same_as<Store<map<double, Store<set<int>>>, urd::sorted>::representation, TreeMapOf<double, Store<set<int>>>>);
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
        const auto counted = urd::testing::Counted();
        fill(7.5);
        fill(8.5);
    }
    if (urd::testing::counts_allocations)
    {
        EXPECT_EQ(urd::testing::counts().allocations, 0);
    }
    EXPECT_EQ(buckets.size(), 2U);
    EXPECT_EQ(buckets.begin()->first, 7.5);
    EXPECT_EQ(buckets.at(8.5).size(), 100U);
}

// Without an order asked for, values that keep their memory get a hash map that recycles: no tree where none is needed.
// Sets with stable references that keep their storage: the node sets, their erased nodes kept for the next elements.
static_assert(std::same_as<Store<set<int>, urd::stable_refs, urd::retains>::representation, urd::repr::RecyclingHashSet<int>>);
static_assert(std::same_as<Store<set<int>, urd::sorted, urd::stable_refs, urd::retains>::representation, urd::repr::RecyclingTreeSet<int>>);
// Without stable_refs, retains is the flat containers' already.
static_assert(std::same_as<Store<set<int>, urd::retains>::representation, gtl::flat_hash_set<int, urd::detail::prehash_hash<int>, urd::detail::prehash_equal<int>>>);

TEST(Recycling, UnorderedMapsRecycleToo)
{
    using Bucket = Store<set<int>, urd::retains>;
    using Groups = Store<map<int, Bucket>>;
    static_assert(std::same_as<Groups::representation, urd::repr::RecyclingHashMap<int, Bucket>>);
    auto groups = Groups();
    for (int round = 0; round < 3; ++round)
    {
        for (int each = 0; each < 50; ++each)
            groups[round].insert(each + (round * 100));
        groups.erase(round);
    }
    {
        const auto counted = urd::testing::Counted();
        for (int each = 0; each < 50; ++each)
            groups[7].insert(each + 700);
    }
    if (urd::testing::counts_allocations)
    {
        EXPECT_EQ(urd::testing::counts().allocations, 0);
    }
    EXPECT_EQ(groups.at(7).size(), 50U);
    EXPECT_FALSE(groups.at(7).contains(0));
}

// A layered map of layered pools (a relation repository): a slot cleared with the map and made again reuses the pool it
// kept, storage and all, where its Store offers clear() (monotone_until_clear) and it is built alike (the same bit
// width): refilling allocates nothing. A monotone pool has no clear(): its slot is built anew.
struct LayeredObject;
struct LayeredPredicate;
using LayeredRow = Store<urd::sequence<urd::index<LayeredObject>>, urd::frozen>;
using LayeredRows = Store<urd::pool<LayeredRow>, urd::monotone_until_clear, urd::bit, urd::layered, urd::concurrent>;
using MonotoneRows = Store<urd::pool<LayeredRow>, urd::monotone, urd::bit, urd::layered>;
template<typename S>
concept clears_rows = requires(S& rows) { rows.clear(); };
static_assert(clears_rows<LayeredRows> && !clears_rows<MonotoneRows>);
using LayeredRelations = Store<map<urd::index<LayeredPredicate>, LayeredRows>, urd::dense, urd::layered, urd::concurrent>;

TEST(LayeredMaps, ClearedSlotsReuseTheirPools)
{
    const auto predicate = [](std::uint32_t at) { return urd::index<LayeredPredicate>(at); };
    const auto row = [](std::uint32_t first, std::uint32_t second)
    { return std::array { urd::index<LayeredObject>(first), urd::index<LayeredObject>(second) }; };
    auto domain = LayeredRelations();
    domain.try_emplace(predicate(0), urd::bit_width { 12 });
    domain.at(predicate(0)).insert(row(0, 0));
    auto state = LayeredRelations();
    state.layer_over(&domain);
    const auto fill = [&]
    {
        for (std::uint32_t at = 0; at < 4; ++at)
        {
            state.try_emplace(predicate(at), urd::bit_width { 12 });
            for (std::uint32_t object = 0; object < 20; ++object)
                state.at(predicate(at)).insert(row(at, object + 1));
        }
    };
    fill();
    state.clear();
    fill();
    state.clear();
    {
        const auto counted = urd::testing::Counted();
        fill();
    }
    if (urd::testing::counts_allocations)
    {
        EXPECT_EQ(urd::testing::counts().allocations, 0);
    }
    // Layered anew: over the domain's pool, numbered after its row, at the width asked.
    EXPECT_EQ(std::as_const(state).at(predicate(0)).base(), 1U);
    EXPECT_EQ(std::as_const(state).at(predicate(0)).size(), 21U);
    EXPECT_EQ(std::as_const(state).at(predicate(1)).base(), 0U);
    EXPECT_EQ(std::as_const(state).at(predicate(1)).width(), urd::bit_width { 12 });
    // Another width builds a new pool.
    state.clear();
    state.try_emplace(predicate(1), urd::bit_width { 20 });
    EXPECT_EQ(std::as_const(state).at(predicate(1)).width(), urd::bit_width { 20 });
    EXPECT_EQ(std::as_const(state).at(predicate(1)).size(), 0U);
}

TEST(LayeredMaps, MonotoneSlotsAreBuiltAnew)
{
    using Relations = Store<map<urd::index<LayeredPredicate>, MonotoneRows>, urd::dense, urd::layered>;
    const auto predicate = urd::index<LayeredPredicate>(0);
    auto domain = Relations();
    domain.try_emplace(predicate, urd::bit_width { 12 });
    domain.at(predicate).insert(std::array { urd::index<LayeredObject>(0), urd::index<LayeredObject>(0) });
    auto state = Relations();
    state.layer_over(&domain);
    for (int round = 0; round < 2; ++round)
    {
        state.clear();
        state.try_emplace(predicate, urd::bit_width { 12 });
        state.at(predicate).insert(std::array { urd::index<LayeredObject>(1), urd::index<LayeredObject>(1) });
        EXPECT_EQ(std::as_const(state).at(predicate).base(), 1U);
        EXPECT_EQ(std::as_const(state).at(predicate).size(), 2U);
    }
}

// A layered map reads a key's value in one walk up the layers (read), or its own layer's only (read_own).
TEST(LayeredMaps, ReadsTheOwnersValueOrTheOwnLayers)
{
    using Layers = Store<map<urd::index<LayeredPredicate>, int>, urd::dense, urd::layered>;
    const auto key = [](std::uint32_t at) { return urd::index<LayeredPredicate>(at); };
    auto root = Layers();
    root.insert_or_assign(key(0), 10);
    auto middle = Layers();
    middle.layer_over(&root);
    middle.insert_or_assign(key(1), 11);
    auto leaf = Layers();
    leaf.layer_over(&middle);
    leaf.insert_or_assign(key(2), 12);
    const auto value = [](int held) { return held; };
    for (std::uint32_t at = 0; at < 3; ++at)
        EXPECT_EQ(leaf.read(key(at), value, -1), static_cast<int>(10 + at));
    EXPECT_EQ(leaf.read(key(3), value, -1), -1);
    EXPECT_EQ(leaf.read_own(key(0), value, -1), -1);
    EXPECT_EQ(leaf.read_own(key(1), value, -1), -1);
    EXPECT_EQ(leaf.read_own(key(2), value, -1), 12);
    EXPECT_EQ(middle.read_own(key(1), value, -1), 11);
}

}  // namespace

// The same with rows by column widths (urd::by_column_widths, a relation repository's rows of objects): a cleared slot made again
// with equal widths reuses its pool, refilling allocates nothing; a row of the parent's narrower columns is found
// through the child's wider ones, as rows hash by their values.
using SchemaLayeredRows = Store<urd::pool<LayeredRow>, urd::monotone_until_clear, urd::by_column_widths, urd::layered, urd::concurrent>;
using SchemaLayeredRelations = Store<map<urd::index<LayeredPredicate>, SchemaLayeredRows>, urd::dense, urd::layered, urd::concurrent>;

TEST(LayeredMaps, ClearedSlotsReuseTheirSchemaPools)
{
    const auto predicate = [](std::uint32_t at) { return urd::index<LayeredPredicate>(at); };
    const auto row = [](std::uint32_t first, std::uint32_t second)
    { return std::array { urd::index<LayeredObject>(first), urd::index<LayeredObject>(second) }; };
    const auto narrow = urd::column_widths(2, 4);
    const auto wide = urd::column_widths(2, 12);
    auto domain = SchemaLayeredRelations();
    domain.try_emplace(predicate(0), narrow);
    domain.at(predicate(0)).insert(row(3, 5));
    auto state = SchemaLayeredRelations();
    state.layer_over(&domain);
    const auto fill = [&]
    {
        for (std::uint32_t at = 0; at < 4; ++at)
        {
            state.try_emplace(predicate(at), wide);
            for (std::uint32_t object = 0; object < 20; ++object)
                state.at(predicate(at)).insert(row(at, object + 100));
        }
    };
    fill();
    state.clear();
    fill();
    state.clear();
    {
        const auto counted = urd::testing::Counted();
        fill();
    }
    if (urd::testing::counts_allocations)
    {
        EXPECT_EQ(urd::testing::counts().allocations, 0);
    }
    const auto& rows = std::as_const(state).at(predicate(0));
    EXPECT_EQ(rows.column_widths(), wide);
    EXPECT_EQ(rows.base(), 1U);
    EXPECT_EQ(rows.size(), 21U);
    // The domain's row, stored in 4-bit columns, found through the state's 12-bit ones, and not stored again.
    EXPECT_EQ(rows.find(row(3, 5)).value, 0U);
    EXPECT_FALSE(state.at(predicate(0)).insert(row(3, 5)).second);
    EXPECT_EQ(rows.view(urd::index<LayeredRow>(0)).get<urd::index<LayeredObject>>(1), urd::index<LayeredObject>(5));
    // A row that does not fit the domain's columns is the state's own.
    EXPECT_EQ(rows.find(row(0, 100)).value, 1U);
    // Other widths build a new pool.
    state.clear();
    state.try_emplace(predicate(1), urd::column_widths(2, 20));
    EXPECT_EQ(std::as_const(state).at(predicate(1)).column_widths(), urd::column_widths(2, 20));
    EXPECT_EQ(std::as_const(state).at(predicate(1)).size(), 0U);
}
