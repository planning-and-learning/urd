// Pools: rows identified by the handles insert gives. Each pool is checked against the rows it was given through
// random inserts and clears: equal rows get equal handles, and every handle gives its row back.

#include "names.hpp"
#include "urd/store.hpp"
#include "verify.hpp"

#include <concepts>
#include <gtest/gtest.h>
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
                                  Store<value<Store<key<int>, urd::sorted>>, urd::handles, urd::monotone>>;
TYPED_TEST_SUITE(Pools, Containers, urd::test::Names);

}  // namespace

// Without structural_sharing, each row is stored once, in full.
static_assert(std::same_as<Store<value<Store<value<int>, urd::frozen>>, urd::handles>::representation, urd::repr::RowInterner<Store<value<int>, urd::frozen>>>);
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
