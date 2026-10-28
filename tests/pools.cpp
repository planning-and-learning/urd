// Pools: rows identified by the handles insert gives. Each pool is checked against the rows it was given through
// random inserts and clears: equal rows get equal handles, and every handle gives its row back.

#include "names.hpp"
#include "urd/store.hpp"
#include "verify.hpp"

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <gtest/gtest.h>
#include <random>
#include <vector>

using urd::key;
using urd::Store;
using urd::value;

namespace
{

template<typename P>
class Pools : public testing::Test
{
};
using Containers = testing::Types<urd::repr::RowInterner<std::vector<int>>,
                                  Store<value<Store<value<int>, urd::frozen>>, urd::handles>,
                                  Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles>,
                                  Store<value<Store<key<int>, urd::sorted>>, urd::handles, urd::monotone>,
                                  urd::repr::TreeVectorSet<std::vector<int>>,
                                  urd::repr::TreeVectorSet<std::vector<double>, 2>,
                                  Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::structural_sharing>,
                                  Store<value<Store<value<double>, urd::frozen>>, urd::handles, urd::structural_sharing>>;
TYPED_TEST_SUITE(Pools, Containers, urd::test::Names);

}  // namespace

// Without structural_sharing, each row is stored once, in full.
static_assert(std::same_as<Store<value<Store<value<int>, urd::frozen>>, urd::handles>::representation, urd::repr::RowInterner<Store<value<int>, urd::frozen>>>);
// With it, sequence rows are trees whose equal subtrees are stored once.
static_assert(std::same_as<Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::structural_sharing>::representation,
                           urd::repr::TreeVectorSet<Store<value<int>, urd::frozen>>>);
// A pool is neither iterated nor counted.
template<typename P>
concept iterable = requires(P pool) { pool.begin(); };
template<typename P>
concept sized = requires(P pool) { pool.size(); };
static_assert(!iterable<Store<value<Store<value<int>, urd::frozen>>, urd::handles>> && !sized<Store<value<Store<value<int>, urd::frozen>>, urd::handles>>);

TYPED_TEST(Pools, OffersWhatItsPropertiesRequire) { static_assert(urd::conforming<TypeParam>); }

TYPED_TEST(Pools, GivesEqualRowsEqualHandlesThroughRandomOperations)
{
    for (unsigned seed = 0; seed < 32; ++seed)
    {
        const auto found = urd::verify(TypeParam(), seed, 300);
        EXPECT_FALSE(found) << found.value_or("");
    }
}

// A set row is equal whatever order its elements came in.
TEST(Pools, SetRowsAreEqualInAnyOrder)
{
    auto facts = Store<value<Store<key<int>, urd::dense, urd::frozen>>, urd::handles>();
    const auto first = facts.insert({ 3, 1, 4 });
    const auto second = facts.insert(std::vector<int> { 4, 3, 1, 1 });
    EXPECT_EQ(first, second);
    EXPECT_NE(first, facts.insert({ 3, 1 }));
    EXPECT_EQ(facts.row_size(first), 3U);
    EXPECT_TRUE(facts.get(first).contains(4));
}

// A row written in k positions of n adds at most k nodes per level of the tree: the paths to the root, no more.
TEST(Pools, WritesShareAllButTheirPaths)
{
    auto random = std::mt19937(31);
    for (const std::uint32_t size : { 1U, 2U, 5U, 64U, 1000U })
    {
        auto pool = urd::repr::TreeVectorSet<std::vector<std::uint32_t>>();
        auto values = std::vector<std::uint32_t>(size);
        std::ranges::generate(values, [&random] { return static_cast<std::uint32_t>(random() % 1000); });
        const auto base = pool.insert(values);
        const auto levels = (static_cast<std::size_t>(std::bit_width(size)) + 1) / 2 + 1;  // ⌈log₄ size⌉ + 1, generously
        for (int round = 0; round < 20; ++round)
        {
            auto writes = std::vector<urd::Write<std::uint32_t>>();
            for (std::uint32_t position = 0; position < size; ++position)
                if (random() % 50 == 0)
                    writes.push_back({ position, static_cast<std::uint32_t>(random()) });
            const auto before = pool.num_nodes();
            const auto written = pool.insert(base, writes);
            EXPECT_LE(pool.num_nodes() - before, writes.size() * levels);
            auto expected = values;
            for (const auto& write : writes)
                expected[write.position] = write.value;
            EXPECT_EQ(written, pool.insert(expected));
        }
    }
}
