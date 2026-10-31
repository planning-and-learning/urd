// Rows of a fixed shape: bits and elements changed in place, nested and tracked. Each header a Store here needs is included.

#include "../allocations.hpp"
#include "urd/compose/family.hpp"
#include "urd/concepts.hpp"
#include "urd/repr/bidirectional_csr.hpp"
#include "urd/repr/csr_multimap.hpp"
#include "urd/repr/dense_multimap.hpp"
#include "urd/repr/dense_set.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/index_map.hpp"
#include "urd/repr/jagged.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/row_pool.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/sparse_set.hpp"
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
/// Rows of bits whose number and lengths are fixed: one array of words, each row a bitset over its part.
using BitRowSets = Store<value<Store<key<std::size_t>, urd::dense>>, urd::fixed_shape>;

// Rows of bits changed at random, against a vector of bools per row: lengths at and around word boundaries, and
// rows of equal length combined.
TEST(FixedShape, ChangeInPlaceLikeVectorsOfBools)
{
    const auto lengths = std::vector<std::size_t> { 0, 1, 63, 64, 65, 130, 64, 65, 130 };
    auto rows = BitRowSets(urd::row_lengths(lengths));
    auto model = std::vector<std::vector<bool>>();
    for (const auto length : lengths)
        model.emplace_back(length, false);
    auto random = std::mt19937(41);
    const auto pick = [&random](std::size_t bound) { return std::uniform_int_distribution<std::size_t>(0, bound - 1)(random); };
    for (int step = 0; step < 5000; ++step)
    {
        const auto row = pick(lengths.size());
        const auto other = pick(lengths.size());
        const auto action = pick(5);
        if (model[row].empty())
            continue;
        const auto bit = pick(model[row].size());
        if (action == 0)
        {
            rows[row].set(bit);
            model[row][bit] = true;
        }
        else if (action == 1)
        {
            rows[row].reset(bit);
            model[row][bit] = false;
        }
        else if (action == 2)
        {
            rows[row].flip();
            model[row].flip();
        }
        else if (lengths[row] == lengths[other])
        {
            if (action == 3)
                rows[row] |= std::as_const(rows)[other];
            else
                rows[row] -= std::as_const(rows)[other];
            for (std::size_t each = 0; each < lengths[row]; ++each)
                model[row][each] = action == 3 ? model[row][each] || model[other][each] : model[row][each] && !model[other][each];
        }
        ASSERT_EQ(rows.size(), lengths.size());
        for (std::size_t each = 0; each < lengths.size(); ++each)
        {
            const auto bits = std::as_const(rows)[each];
            ASSERT_EQ(bits.size(), lengths[each]);
            ASSERT_EQ(bits.count(), static_cast<std::size_t>(std::ranges::count(model[each], true)));
            for (std::size_t position = 0; position < lengths[each]; ++position)
                ASSERT_EQ(bits.test(position), model[each][position]) << "row " << each << " bit " << position;
        }
    }
    auto copy = rows;
    EXPECT_EQ(copy, rows);
    if (lengths[5] > 0)
        copy[5].flip(0);
    EXPECT_NE(copy, rows);
}

// Built from sets of indices, each row is as long as its largest index needs.
TEST(FixedShape, BuildFromSets)
{
    using Row = Store<key<std::size_t>, urd::dense>;
    const auto sets = std::vector<Row> { Row { 3, 70 }, Row {}, Row { 0 } };
    const auto rows = BitRowSets(sets.begin(), sets.end());
    ASSERT_EQ(rows.size(), 3U);
    EXPECT_EQ(rows[0].size(), 71U);
    EXPECT_EQ(rows[0].count(), 2U);
    EXPECT_TRUE(rows[0].test(3) && rows[0].test(70));
    EXPECT_EQ(rows[1].size(), 0U);
    EXPECT_EQ(rows[2].size(), 1U);
    EXPECT_TRUE(rows[2].test(0));
}

// Rows of elements of fixed lengths, changed in place: spans into one array.
// nonempty through the shape constructors too: a fixed-shape Store is nonempty when it has a row, whatever its length.
TEST(FixedShape, NonemptyNeedsARow)
{
    using Rows = Store<value<Store<value<int>>>, urd::fixed_shape, urd::nonempty>;
    static_assert(!std::default_initializable<Rows>);
    EXPECT_FALSE(Rows(urd::row_lengths { 0 }).empty());
    EXPECT_DEBUG_DEATH((void) Rows(urd::row_lengths {}), "nonempty");
    using Nested = Store<value<Store<value<Store<value<int>>>, urd::fixed_shape>>, urd::fixed_shape, urd::nonempty>;
    EXPECT_FALSE(Nested(std::vector<std::vector<std::size_t>> { { 2, 3 } }).empty());
    EXPECT_DEBUG_DEATH((void) Nested(std::vector<std::vector<std::size_t>> {}), "nonempty");
}

TEST(FixedShape, RowsOfElementsChangeInPlace)
{
    using Rows = Store<value<Store<value<double>>>, urd::fixed_shape>;
    static_assert(std::same_as<Rows::reference, std::span<double>>);
    auto rows = Rows(urd::row_lengths { 2, 0, 3 });
    rows[2][1] = 3.5;
    rows[0][0] = 1.0;
    const auto& read = rows;
    EXPECT_EQ(read[0].size(), 2U);
    EXPECT_EQ(read[1].size(), 0U);
    EXPECT_EQ(read[2][1], 3.5);
    EXPECT_EQ(read[0][0], 1.0);
    EXPECT_EQ(read[2][0], 0.0);
    auto copy = rows;
    EXPECT_EQ(copy, rows);
    copy[2][2] = 1.0;
    EXPECT_NE(copy, rows);
}

// Rows of rows of bits (vertex × partition × bits), changed at random against vectors of vectors of bools; reset
// unsets every bit and keeps the shape.
TEST(FixedShape, NestedRowsOfBits)
{
    using Graph = Store<value<Store<value<Store<key<std::size_t>, urd::dense>>, urd::fixed_shape>>, urd::fixed_shape>;
    static_assert(std::same_as<Graph::representation, urd::repr::Jagged<Store<value<Store<key<std::size_t>, urd::dense>>, urd::fixed_shape>>>);
    const auto shape = std::vector<std::vector<std::size_t>> { { 64, 65, 1 }, { 130 }, {}, { 3, 64 } };
    auto graph = Graph(shape);
    auto model = std::vector<std::vector<std::vector<bool>>>();
    for (const auto& lengths : shape)
    {
        auto& partitions = model.emplace_back();
        for (const auto length : lengths)
            partitions.emplace_back(length, false);
    }
    auto random = std::mt19937(47);
    const auto pick = [&random](std::size_t bound) { return std::uniform_int_distribution<std::size_t>(0, bound - 1)(random); };
    const auto check = [&graph, &model]
    {
        const auto& read = graph;
        ASSERT_EQ(read.size(), model.size());
        for (std::size_t vertex = 0; vertex < model.size(); ++vertex)
        {
            ASSERT_EQ(static_cast<std::size_t>(std::ranges::size(read[vertex])), model[vertex].size());
            for (std::size_t partition = 0; partition < model[vertex].size(); ++partition)
            {
                const auto bits = read[vertex][static_cast<std::ptrdiff_t>(partition)];
                ASSERT_EQ(bits.size(), model[vertex][partition].size());
                for (std::size_t bit = 0; bit < bits.size(); ++bit)
                    ASSERT_EQ(bits.test(bit), model[vertex][partition][bit]) << vertex << " " << partition << " " << bit;
            }
        }
    };
    for (int step = 0; step < 3000; ++step)
    {
        const auto vertex = pick(model.size());
        if (model[vertex].empty())
            continue;
        const auto partition = pick(model[vertex].size());
        const auto bit = pick(model[vertex][partition].size());
        const auto value = pick(2) == 0;
        graph[vertex][static_cast<std::ptrdiff_t>(partition)].set(bit, value);
        model[vertex][partition][bit] = value;
        if (step % 100 == 0)
            check();
    }
    check();
    graph.reset();
    for (auto& partitions : model)
        for (auto& bits : partitions)
            std::fill(bits.begin(), bits.end(), false);
    check();
}

// With proportional_clear, reset touches only the rows handed out to change since the last one: a few of many rows
// reset far faster than all of them, and the rest stay as they were.
TEST(FixedShape, TrackedResetCostsTheRowsHandedOut)
{
    using Row = Store<key<std::size_t>, urd::dense>;
    using Tracked = Store<value<Row>, urd::fixed_shape, urd::proportional_clear>;
    using Untracked = Store<value<Row>, urd::fixed_shape>;
    static_assert(std::same_as<Tracked::representation, urd::repr::Jagged<Row, true>>);
    static_assert(std::same_as<Untracked::representation, urd::repr::Jagged<Row>>);
    constexpr std::size_t rows = 100000;
    const auto lengths = urd::row_lengths(std::vector<std::size_t>(rows, 640));
    auto tracked = Tracked(lengths);
    auto untracked = Untracked(lengths);
    const auto timed = [](auto& store)
    {
        auto best = std::numeric_limits<std::int64_t>::max();
        for (int round = 0; round < 7; ++round)
        {
            store[3].set(5);
            store[rows - 1].set(639);
            const auto start = std::chrono::steady_clock::now();
            store.reset();
            const auto stop = std::chrono::steady_clock::now();
            best = std::min(best, static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count()));
            EXPECT_FALSE(std::as_const(store)[3].test(5));
            EXPECT_FALSE(std::as_const(store)[rows - 1].test(639));
        }
        return best;
    };
    const auto few = timed(tracked);
    const auto all = timed(untracked);
    EXPECT_LT(few * 10, all) << "tracked " << few << " ns, untracked " << all << " ns";
}

// Three levels deep: rows of rows of rows of values, each level a range over the one below, all in one array.
TEST(FixedShape, ThreeLevels)
{
    using Cube = Store<value<Store<value<Store<value<Store<value<int>>>, urd::fixed_shape>>, urd::fixed_shape>>, urd::fixed_shape>;
    const auto shape = std::vector<std::vector<std::vector<std::size_t>>> { { { 2, 3 }, { 1 } }, {}, { { 4 } } };
    auto cube = Cube(shape);
    ASSERT_EQ(cube.size(), 3U);
    auto layer = cube[0];
    auto row = layer[0];
    ASSERT_EQ(std::ranges::size(row), 2);
    row[1][2] = 7;
    cube[2][0][0][3] = 9;
    const auto& read = cube;
    EXPECT_EQ(read[0][0][1][2], 7);
    EXPECT_EQ(read[2][0][0][3], 9);
    EXPECT_EQ(read[0][1][0].size(), 1U);
    EXPECT_EQ(read[1].size(), 0);
    cube.reset();
    EXPECT_EQ(read[0][0][1][2], 0);
    EXPECT_EQ(read[2][0][0][3], 0);
}

}  // namespace
