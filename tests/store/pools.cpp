// Interners, partitions and pools of records. Each family header a Store here needs is included, and no other.

#include "../allocations.hpp"
#include "urd/compose/family.hpp"
#include "urd/concepts.hpp"
#include "urd/store.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/records.hpp"

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

// An interner: each new element gets the next index, which stays its index; a known one keeps its index.
TEST(Interner, GivesEachElementItsIndex)
{
    auto symbols = Store<key<std::string>, urd::positional, urd::monotone>();
    const auto index = [&symbols](const std::string& symbol) { return symbols.position(symbols.insert(symbol).first); };
    EXPECT_EQ(index("on"), 0U);
    EXPECT_EQ(index("clear"), 1U);
    EXPECT_EQ(index("on"), 0U);
    EXPECT_EQ(symbols[1], "clear");
    EXPECT_EQ(symbols.position(symbols.find("clear")), 1U);
    EXPECT_EQ(symbols.position(symbols.find("holding")), symbols.size());
    EXPECT_EQ(std::vector<std::string>(symbols.rbegin(), symbols.rend()), (std::vector<std::string> { "clear", "on" }));
}

// A partition says whether a key is an element.
TEST(Partition, Contains)
{
    auto classes = Store<key<int>, urd::partition>();
    classes.insert(3);
    EXPECT_TRUE(classes.contains(0) && classes.contains(3) && !classes.contains(4));
}

// A pool of records visits them in index order.
TEST(Records, Iterate)
{
    struct Name
    {
        int id = 0;
        static constexpr auto identifying_members() noexcept { return urd::Members<&Name::id> {}; }
    };
    auto names = Store<value<Name>, urd::handles, urd::unique, urd::flat>();
    names.insert(Name { 7 });
    names.insert(Name { 2 });
    names.insert(Name { 7 });
    auto ids = std::vector<int>();
    for (const auto& name : names)
        ids.push_back(name.id);
    EXPECT_EQ(ids, (std::vector { 7, 2 }));
}

// A pool of records counts a record as a set does: once, or not at all.
TEST(Records, Count)
{
    struct Point
    {
        int x = 0;
        static constexpr auto identifying_members() noexcept { return urd::Members<&Point::x> {}; }
    };
    auto points = Store<value<Point>, urd::handles, urd::unique, urd::flat>();
    points.insert(Point { 1 });
    EXPECT_EQ(points.count(Point { 1 }), 1U);
    EXPECT_EQ(points.count(Point { 2 }), 0U);
}

// A partition: union-find over indices, by request.
static_assert(std::same_as<Store<key<int>, urd::partition>::representation, urd::repr::DisjointSets<int>>);

// A partition answers what a labeling recomputed after each merge answers.
TEST(Partition, AgreesWithRelabeling)
{
    auto random = std::mt19937(17);
    for (int round = 0; round < 50; ++round)
    {
        constexpr std::size_t size = 24;
        auto partition = Store<key<int>, urd::partition>();
        partition.insert(static_cast<int>(size) - 1);
        auto label = std::vector<std::size_t>(size);
        std::iota(label.begin(), label.end(), std::size_t { 0 });
        for (int step = 0; step < 40; ++step)
        {
            const auto lhs = static_cast<int>(random() % size);
            const auto rhs = static_cast<int>(random() % size);
            const auto old = label[static_cast<std::size_t>(rhs)];
            const auto joined = label[static_cast<std::size_t>(lhs)];
            EXPECT_EQ(partition.merge(lhs, rhs), old != joined);
            std::ranges::replace(label, old, joined);
            for (int each = 0; each < static_cast<int>(size); ++each)
            {
                ASSERT_EQ(partition.same(lhs, each), label[static_cast<std::size_t>(lhs)] == label[static_cast<std::size_t>(each)]);
                ASSERT_EQ(partition.class_size(each), static_cast<std::size_t>(std::ranges::count(label, label[static_cast<std::size_t>(each)])));
                ASSERT_TRUE(partition.same(partition.find(each), each));
            }
            auto distinct = label;
            std::ranges::sort(distinct);
            ASSERT_EQ(partition.classes(), static_cast<std::size_t>(std::ranges::distance(distinct.begin(), std::ranges::unique(distinct).begin())));
        }
    }
}

// An interner's table hashes through its own elements: copies and moves each keep theirs, and a moved-from interner
// starts empty and works.
TEST(Interner, CopiesAndMovesKeepTheirOwnIndex)
{
    using Symbols = Store<key<std::string>, urd::positional>;
    auto symbols = Symbols { "on", "clear" };
    auto copy = symbols;
    copy.insert("holding");
    EXPECT_FALSE(symbols.contains("holding"));
    auto moved = std::move(symbols);
    EXPECT_EQ(moved.position(moved.find("clear")), 1U);
    symbols.clear();  // a moved-from Store is valid; clearing it is how code reuses one
    symbols.insert("empty");
    EXPECT_EQ(symbols.size(), 1U);
    EXPECT_EQ(symbols.position(symbols.find("empty")), 0U);
    EXPECT_FALSE(moved.contains("empty"));
    copy = moved;
    EXPECT_FALSE(copy.contains("holding"));
    EXPECT_EQ(copy.position(copy.find("clear")), 1U);
}

// Integer elements of the same width as the positions the table holds are never taken for positions.
TEST(Interner, IntegerElementsAreNotPositions)
{
    auto ids = Store<key<std::uint32_t>, urd::positional>();
    for (const std::uint32_t id : { 2U, 0U, 1U, 0U, 7U })
        ids.insert(id);
    EXPECT_EQ(ids.size(), 4U);
    EXPECT_EQ(ids.position(ids.find(0U)), 1U);
    EXPECT_EQ(ids.position(ids.find(1U)), 2U);
    EXPECT_EQ(ids.position(ids.find(3U)), ids.size());
}

// Threads interning overlapping elements at once: each element gets one position, the same for every thread, and
// positions run 0..n-1 without gaps.
TEST(Interner, ThreadsAgreeOnPositions)
{
    auto symbols = Store<key<int>, urd::positional, urd::monotone, urd::concurrent>();
    constexpr int threads = 4;
    constexpr int each = 5000;
    auto seen = std::vector<std::vector<std::size_t>>(threads, std::vector<std::size_t>(each));
    {
        auto workers = std::vector<std::jthread>();
        for (int thread = 0; thread < threads; ++thread)
            workers.emplace_back(
                [&symbols, &seen, thread]
                {
                    // Every thread inserts the same elements, each starting at another point.
                    for (int step = 0; step < each; ++step)
                    {
                        const int element = (step + thread * each / threads) % each;
                        seen[static_cast<std::size_t>(thread)][static_cast<std::size_t>(element)] = symbols.position(symbols.insert(element).first);
                    }
                });
    }
    EXPECT_EQ(symbols.size(), static_cast<std::size_t>(each));
    for (int thread = 1; thread < threads; ++thread)
        EXPECT_EQ(seen[static_cast<std::size_t>(thread)], seen[0]);
    auto positions = seen[0];
    std::ranges::sort(positions);
    for (std::size_t position = 0; position < positions.size(); ++position)
        ASSERT_EQ(positions[position], position);
    for (int element = 0; element < each; ++element)
        ASSERT_EQ(symbols[seen[0][static_cast<std::size_t>(element)]], element);
}

}  // namespace
