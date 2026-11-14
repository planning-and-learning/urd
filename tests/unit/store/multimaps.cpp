// Multimaps: the values of a key in order, compressed sparse rows, graphs that answer by value too, and rebuilding. Each family header a Store here
// needs is included, and no other.

#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/flat_map.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/sequences/jagged.hpp"
#include "urd/repr/sequences/row_pool.hpp"
#include "urd/repr/sequences/segmented.hpp"
#include "urd/repr/sequences/sequence.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/flat_set.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/values/std_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/repr/wrappers/recycling.hpp"
#include "urd/store.hpp"
#include "urd/testing/allocations.hpp"

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

using urd::map;
using urd::sequence;
using urd::set;
using urd::Store;

// The values of a key, in the order given: a span of a CSR multimap, a view of any other multimap's equal_range.
/// The values of `key`, by equal_range, as with the standard's multimaps.
template<typename Map>
std::vector<int> values_of(const Map& map, int key)
{
    auto result = std::vector<int>();
    for (auto [it, last] = map.equal_range(key); it != last; ++it)
        result.push_back((*it).second);
    return result;
}

// equal_range gives a key's values and stops there, whatever follows: keys without values in between are not walked.
template<typename Map>
void check_gaps()
{
    const auto pairs = std::vector<std::pair<int, int>> { { 1, 10 }, { 1, 11 }, { 6, 60 } };
    const auto map = Map(pairs.begin(), pairs.end());
    EXPECT_EQ(values_of(map, 1), (std::vector<int> { 10, 11 }));
    EXPECT_EQ(values_of(map, 6), (std::vector<int> { 60 }));
    for (const int absent : { 0, 3, 5, 7, 9 })
        EXPECT_TRUE(values_of(map, absent).empty()) << absent;
    auto all = std::vector<std::pair<int, int>>();
    for (const auto& [key, value] : map)
        all.emplace_back(key, value);
    EXPECT_EQ(all, pairs);
    EXPECT_EQ(std::distance(map.begin(), map.end()), 3);
    EXPECT_EQ(map.equal_range(3).first, map.equal_range(3).second);
    EXPECT_EQ(map.equal_range(6).second, map.end());
}
TEST(MultiValues, EqualRangeOverKeysWithoutValues)
{
    check_gaps<Store<map<int, int>, urd::multi, urd::dense>>();
    check_gaps<Store<map<int, int>, urd::multi, urd::dense, urd::frozen>>();
}

TEST(MultiValues, GivesEachKeysValuesInOrder)
{
    const auto pairs = std::vector<std::pair<int, int>> { { 2, 20 }, { 0, 1 }, { 2, 21 }, { 0, 2 }, { 2, 22 } };
    const auto csr = Store<map<int, int>, urd::multi, urd::dense, urd::frozen>(pairs.begin(), pairs.end());
    const auto tree = Store<map<int, int>, urd::multi, urd::sorted>(pairs.begin(), pairs.end());
    const auto postings = Store<map<int, int>, urd::multi>(pairs.begin(), pairs.end());
    const auto nodes = Store<map<int, int>, urd::multi, urd::stable_refs>(pairs.begin(), pairs.end());
    EXPECT_TRUE(std::ranges::equal(values_of(postings, 2), std::vector<int> { 20, 21, 22 }));
    EXPECT_TRUE(std::ranges::equal(values_of(tree, 2), std::vector<int> { 20, 21, 22 }));
    EXPECT_EQ(std::ranges::distance(values_of(nodes, 2)), 3);
    EXPECT_TRUE(std::ranges::empty(values_of(nodes, 7)));
    EXPECT_EQ(std::ranges::distance(nodes.equal_range(2).first, nodes.equal_range(2).second), 3);
    EXPECT_EQ(std::ranges::distance(tree.equal_range(2).first, tree.equal_range(2).second), 3);
    EXPECT_TRUE(std::ranges::equal(values_of(csr, 2), std::vector<int> { 20, 21, 22 }));
    EXPECT_TRUE(std::ranges::equal(values_of(csr, 0), std::vector<int> { 1, 2 }));
    EXPECT_TRUE(values_of(csr, 1).empty());
    EXPECT_TRUE(values_of(csr, 7).empty());
    EXPECT_EQ(csr.size(), 5U);
}

// Built from pairs in any order, a CSR multimap holds what std::multimap holds, in the same order: frozen
// containers get no random operations, so their construction is compared here.
TEST(MultiValues, CsrBuildsWhatAMultimapHolds)
{
    auto random = std::mt19937(11);
    for (int round = 0; round < 200; ++round)
    {
        auto pairs = std::vector<std::pair<int, int>>();
        for (int count = static_cast<int>(random() % 40); count > 0; --count)
            pairs.emplace_back(static_cast<int>(random() % 12), static_cast<int>(random() % 100));
        const auto csr = Store<map<int, int>, urd::multi, urd::dense, urd::frozen, urd::sorted>(pairs.begin(), pairs.end());
        const auto expected = std::multimap<int, int>(pairs.begin(), pairs.end());
        ASSERT_TRUE(std::ranges::equal(csr, expected, [](const auto& lhs, const auto& rhs) { return lhs.first == rhs.first && lhs.second == rhs.second; }));
        for (int each = -1; each < 14; ++each)
        {
            EXPECT_EQ(csr.count(each), expected.count(each));
            EXPECT_EQ(std::ranges::distance(csr.lower_bound(each), csr.end()), std::ranges::distance(expected.lower_bound(each), expected.end()));
        }
        auto backwards = std::vector<int>();
        for (auto it = csr.end(); it != csr.begin();)
            backwards.push_back((--it)->first);
        EXPECT_TRUE(std::ranges::equal(backwards, expected | std::views::keys | std::views::reverse));
    }
}

// A frozen graph: a multimap from sources to targets that answers by value too, so successors are a vertex's values
// and predecessors its keys, each in the order the edges were given.
static_assert(
    std::same_as<Store<map<int, int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>::representation, urd::repr::BidirectionalCsr<int, int>>);

// Built from pairs in any order, the keys of each value are those paired with it, in the order given.
TEST(Graph, KeysOfAValueAreItsPairsInOrder)
{
    auto random = std::mt19937(37);
    for (int round = 0; round < 200; ++round)
    {
        auto pairs = std::vector<std::pair<int, int>>();
        for (int count = static_cast<int>(random() % 40); count > 0; --count)
            pairs.emplace_back(static_cast<int>(random() % 12), static_cast<int>(random() % 12));
        const auto graph = Store<map<int, int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>(pairs.begin(), pairs.end());
        for (int target = 0; target < 14; ++target)
        {
            auto expected = std::vector<int>();
            for (const auto& [source, each] : pairs)
                if (each == target)
                    expected.push_back(source);
            ASSERT_TRUE(std::ranges::equal(graph.keys(target), expected)) << "round " << round << ", value " << target;
        }
    }
}

TEST(Graph, GivesSuccessorsAndPredecessors)
{
    const auto edges = std::vector<std::pair<int, int>> { { 2, 0 }, { 0, 1 }, { 2, 1 }, { 1, 2 }, { 0, 2 } };
    const auto graph = Store<map<int, int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>(edges.begin(), edges.end());
    EXPECT_EQ(graph.size(), 5U);
    EXPECT_TRUE(std::ranges::equal(values_of(graph, 0), std::vector<int> { 1, 2 }));
    EXPECT_TRUE(std::ranges::equal(values_of(graph, 2), std::vector<int> { 0, 1 }));
    EXPECT_TRUE(std::ranges::equal(graph.keys(1), std::vector<int> { 0, 2 }));
    EXPECT_TRUE(std::ranges::equal(graph.keys(2), std::vector<int> { 1, 0 }));
    EXPECT_TRUE(values_of(graph, 3).empty() && graph.keys(3).empty());
}

// Frozen rows rebuild in their arrays by assign, a sequence's; a Store of another kind is built anew, with the same result.
TEST(Assign, RebuildsRowsAndOthers)
{
    using Rows = Store<sequence<Store<sequence<int>, urd::frozen>>, urd::frozen>;
    using Row = Store<sequence<int>, urd::frozen>;
    auto rows = Rows { Row { 1, 2, 3 }, Row { 4 } };
    const auto next = std::vector<Row> { Row { 5 }, Row {}, Row { 6, 7 } };
    rows.assign(next.begin(), next.end());
    EXPECT_EQ(rows, Rows(next.begin(), next.end()));
    using Sorted = Store<set<int>, urd::sorted, urd::frozen>;
    auto sorted = Sorted { 3, 1 };
    const auto values = std::vector<int> { 9, 4, 4 };
    sorted = Sorted(values.begin(), values.end());  // assign is a sequence's: any other Store is built anew
    EXPECT_EQ(sorted, (Sorted { 4, 9 }));
    using Successors = Store<map<int, int>, urd::multi, urd::dense, urd::frozen>;
    const auto pairs = std::vector<std::pair<int, int>> { { 2, 7 }, { 0, 1 }, { 2, 8 } };
    auto graph = Successors();
    graph = Successors(pairs.begin(), pairs.end());
    EXPECT_TRUE(std::ranges::equal(values_of(graph, 2), std::vector<int> { 7, 8 }));
    // A multimap that is refilled is not frozen: clear keeps its rows' storage, insert fills them again.
    auto refilled = Store<map<int, int>, urd::multi, urd::dense>(pairs.begin(), pairs.end());
    refilled.clear();
    refilled.emplace(2, 9);
    EXPECT_TRUE(std::ranges::equal(values_of(refilled, 2), std::vector<int> { 9 }));
    EXPECT_TRUE(values_of(refilled, 0).empty());
}

// What every sequence has, rows too: resize and assign, as std::vector's; and what contiguous storage tells of its room.
TEST(Sequences, ResizeAndAssignWhateverHoldsThem)
{
    using Row = Store<sequence<int>, urd::frozen>;
    auto jagged = Store<sequence<Row>>();
    jagged.push_back(Row { 1, 2 });
    jagged.resize(3);
    ASSERT_EQ(jagged.size(), 3U);
    EXPECT_TRUE(std::ranges::empty(jagged[2]));
    jagged.resize(4, Row { 7 });
    EXPECT_TRUE(std::ranges::equal(jagged[3], std::vector<int> { 7 }));
    jagged.resize(1);
    EXPECT_EQ(jagged.size(), 1U);
    jagged.assign(2, Row { 5, 6 });
    ASSERT_EQ(jagged.size(), 2U);
    EXPECT_TRUE(std::ranges::equal(jagged[1], std::vector<int> { 5, 6 }));

    using Kept = Store<sequence<int>, urd::retains>;
    auto pool = Store<sequence<Kept>, urd::retains>();
    pool.resize(2);
    pool.resize(3, Kept { 4 });
    ASSERT_EQ(pool.size(), 3U);
    EXPECT_TRUE(pool[0].empty());
    EXPECT_EQ(pool[2], (Kept { 4 }));
    pool.assign(2, Kept { 8, 9 });
    ASSERT_EQ(pool.size(), 2U);
    EXPECT_EQ(pool[1], (Kept { 8, 9 }));
    const auto rows = std::vector<Kept> { Kept { 1 } };
    pool.assign(rows.begin(), rows.end());
    EXPECT_EQ(pool.size(), 1U);
    EXPECT_GE(pool.capacity(), 1U);
    pool.shrink_to_fit();
    EXPECT_GE(pool.max_size(), pool.capacity());

    auto segments = Store<sequence<int>, urd::stable_refs>();
    segments.assign(3, 7);
    EXPECT_EQ(segments.size(), 3U);
    const auto values = std::vector<int> { 1, 2 };
    segments.assign(values.begin(), values.end());
    EXPECT_TRUE(std::ranges::equal(segments, values));

    auto indexed = Store<set<int>, urd::insertion_ordered>(urd::layer { 0 });
    indexed.reserve(100);
    EXPECT_GE(indexed.capacity(), 100U);
    EXPECT_GE(indexed.max_size(), indexed.capacity());
    indexed.shrink_to_fit();
    auto sparse = Store<set<int>, urd::dense, urd::insertion_ordered>(urd::layer { 0 });
    sparse.reserve(100);
    EXPECT_GE(sparse.capacity(), 100U);
    sparse.shrink_to_fit();
}

}  // namespace

// The standard's equal_range, ordered or not: a multimap's values of a key; a unique container's one element or none.
TEST(Multimap, EqualRangeOfAnUnorderedMultimapAndOfAUniqueSet)
{
    auto edges = urd::Store<urd::map<int, int>, urd::multi, urd::retains>();
    edges.insert({ 1, 10 });
    edges.insert({ 2, 20 });
    edges.insert({ 1, 11 });
    const auto [first, last] = edges.equal_range(1);
    auto values = std::vector<int>();
    for (auto it = first; it != last; ++it)
        values.push_back(it->second);
    std::ranges::sort(values);
    EXPECT_EQ(values, (std::vector<int> { 10, 11 }));
    const auto found = std::find_if(first, last, [](const auto& entry) { return entry.second == 10; });
    edges.erase(found);
    EXPECT_EQ(edges.count(1), 1U);
    EXPECT_EQ(edges.size(), 2U);

    const auto seen = urd::Store<urd::set<int>> { 3, 5 };
    EXPECT_EQ(std::distance(seen.equal_range(3).first, seen.equal_range(3).second), 1);
    EXPECT_EQ(seen.equal_range(4).first, seen.equal_range(4).second);
}
