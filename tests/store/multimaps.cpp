// Multimaps: the values of a key in order, compressed sparse rows, graphs that answer by value too, and rebuilding by assign. Each family header a Store here
// needs is included, and no other.

#include "../allocations.hpp"
#include "urd/compose/family.hpp"
#include "urd/concepts.hpp"
#include "urd/repr/bidirectional_csr.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/csr_multimap.hpp"
#include "urd/repr/dense_multimap.hpp"
#include "urd/repr/dense_set.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/entry_ordered.hpp"
#include "urd/repr/flat_map.hpp"
#include "urd/repr/flat_set.hpp"
#include "urd/repr/index_map.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/jagged.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/recycling.hpp"
#include "urd/repr/row_pool.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
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

// The values of a key, in the order given: a span of a CSR multimap, a view of any other multimap's equal_range.
TEST(MultiValues, GivesEachKeysValuesInOrder)
{
    const auto pairs = std::vector<std::pair<int, int>> { { 2, 20 }, { 0, 1 }, { 2, 21 }, { 0, 2 }, { 2, 22 } };
    const auto csr = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen>(pairs.begin(), pairs.end());
    const auto tree = Store<key<int>, value<int>, urd::multi, urd::sorted>(pairs.begin(), pairs.end());
    const auto postings = Store<key<int>, value<int>, urd::multi>(pairs.begin(), pairs.end());
    const auto nodes = Store<key<int>, value<int>, urd::multi, urd::stable_refs>(pairs.begin(), pairs.end());
    EXPECT_TRUE(std::ranges::equal(postings.values(2), std::vector<int> { 20, 21, 22 }));
    // std::unordered_multimap keeps equal keys together, in no promised order.
    auto node_values = std::vector<int>(nodes.values(2).begin(), nodes.values(2).end());
    std::ranges::sort(node_values);
    EXPECT_EQ(node_values, (std::vector<int> { 20, 21, 22 }));
    EXPECT_TRUE(std::ranges::equal(csr.values(2), std::vector<int> { 20, 21, 22 }));
    EXPECT_TRUE(std::ranges::equal(tree.values(2), std::vector<int> { 20, 21, 22 }));
    EXPECT_TRUE(std::ranges::equal(csr.values(0), std::vector<int> { 1, 2 }));
    EXPECT_TRUE(csr.values(1).empty());
    EXPECT_TRUE(csr.values(7).empty());
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
        const auto csr = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::sorted>(pairs.begin(), pairs.end());
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
    std::same_as<Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>::representation, urd::repr::BidirectionalCsr<int, int>>);

// Built from pairs in any order, the keys of each value are those paired with it, in the order given.
TEST(Graph, KeysOfAValueAreItsPairsInOrder)
{
    auto random = std::mt19937(37);
    for (int round = 0; round < 200; ++round)
    {
        auto pairs = std::vector<std::pair<int, int>>();
        for (int count = static_cast<int>(random() % 40); count > 0; --count)
            pairs.emplace_back(static_cast<int>(random() % 12), static_cast<int>(random() % 12));
        const auto graph = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>(pairs.begin(), pairs.end());
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
    const auto graph = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>(edges.begin(), edges.end());
    EXPECT_EQ(graph.size(), 5U);
    EXPECT_TRUE(std::ranges::equal(graph.values(0), std::vector<int> { 1, 2 }));
    EXPECT_TRUE(std::ranges::equal(graph.values(2), std::vector<int> { 0, 1 }));
    EXPECT_TRUE(std::ranges::equal(graph.keys(1), std::vector<int> { 0, 2 }));
    EXPECT_TRUE(std::ranges::equal(graph.keys(2), std::vector<int> { 1, 0 }));
    EXPECT_TRUE(graph.values(3).empty() && graph.keys(3).empty());
}

// A frozen Store changes only by assign, which rebuilds in the storage held where the representation can: compressed
// sparse rows rebuilt from pairs of the same size allocate nothing, and hold what a fresh build would.
TEST(Assign, RebuildsCsrInPlace)
{
    using Successors = Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen>;
    auto random = std::mt19937(43);
    const auto pairs = [&random]
    {
        auto drawn = std::vector<std::pair<int, int>>();
        for (int each = 0; each < 500; ++each)
            drawn.emplace_back(std::uniform_int_distribution<int>(0, 49)(random), each);
        drawn.emplace_back(49, -1);  // the same key range each time
        return drawn;
    };
    auto graph = Successors();
    const auto first = pairs();
    graph.assign(first.begin(), first.end());
    EXPECT_EQ(graph, Successors(first.begin(), first.end()));
    for (int round = 0; round < 5; ++round)
    {
        const auto next = pairs();
        {
            const auto counted = urd::test::Counted();
            graph.assign(next.begin(), next.end());
        }
        if (urd::test::counts_allocations)
        {
            EXPECT_EQ(urd::test::allocations(), 0) << "round " << round;
        }
        EXPECT_EQ(graph, Successors(next.begin(), next.end())) << "round " << round;
    }
    const auto fewer = std::vector<std::pair<int, int>> { { 2, 7 }, { 0, 1 }, { 2, 8 } };
    graph.assign(fewer.begin(), fewer.end());
    EXPECT_EQ(graph, Successors(fewer.begin(), fewer.end()));
    EXPECT_EQ(std::vector<int>(graph.values(2).begin(), graph.values(2).end()), (std::vector<int> { 7, 8 }));
}

// Frozen rows rebuild in their arrays; a representation without assign is built anew, with the same result.
TEST(Assign, RebuildsRowsAndOthers)
{
    using Rows = Store<value<Store<value<int>, urd::frozen>>, urd::frozen>;
    using Row = Store<value<int>, urd::frozen>;
    auto rows = Rows { Row { 1, 2, 3 }, Row { 4 } };
    const auto next = std::vector<Row> { Row { 5 }, Row {}, Row { 6, 7 } };
    rows.assign(next.begin(), next.end());
    EXPECT_EQ(rows, Rows(next.begin(), next.end()));
    using Sorted = Store<key<int>, urd::sorted, urd::frozen>;
    auto sorted = Sorted { 3, 1 };
    const auto values = std::vector<int> { 9, 4, 4 };
    sorted.assign(values.begin(), values.end());
    EXPECT_EQ(sorted, (Sorted { 4, 9 }));
}

}  // namespace
