// Nested Stores, whose rows are Stores: each behaves like the same std::vector of std::vectors under random
// inserts and erases of rows, sorted rows answer lookups by position, and a Store that retains at every level
// allocates nothing when refilled. One typed test per behavior, for every nested Store in the list.

#include "allocations.hpp"
#include "names.hpp"
#include "urd/store.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <random>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using urd::Store;
using urd::value;
using urd::test::allocations;
using urd::test::Counted;

using Leaf = Store<value<int>, urd::frozen>;
using SortedLeaf = Store<value<int>, urd::sorted, urd::frozen>;
using Middle = Store<value<Leaf>, urd::frozen>;

using Nested = testing::Types<Store<value<Leaf>>,                                           // flattened rows
                              Store<value<SortedLeaf>>,                                     // flattened, each row sorted
                              Store<value<Middle>>,                                         // three levels, flattened
                              Store<value<Leaf>, urd::retains>,                             // flattened, storage kept
                              Store<value<Store<value<int>, urd::retains>>, urd::retains>,  // rows kept for reuse
                              Store<value<Store<value<int>>>>>;                             // a vector of Stores

template<typename S>
class Rows : public testing::Test
{
};
TYPED_TEST_SUITE(Rows, Nested, urd::test::Names);

/// The contents as std::vectors, level by level.
template<typename R>
auto materialize(const R& range)
{
    using E = std::remove_cvref_t<std::ranges::range_reference_t<const R>>;
    if constexpr (std::ranges::range<E>)
    {
        auto result = std::vector<decltype(materialize(std::declval<const E&>()))>();
        for (const auto& row : range)
            result.push_back(materialize(row));
        return result;
    }
    else
        return std::vector<E>(std::ranges::begin(range), std::ranges::end(range));
}

/// A row of up to four elements, each a row itself at a deeper level, else a digit.
template<typename Row>
Row random_row(std::mt19937& random)
{
    using E = typename Row::value_type;
    auto elements = std::vector<E>();
    const int length = std::uniform_int_distribution<int>(0, 4)(random);
    for (int pos = 0; pos < length; ++pos)
    {
        if constexpr (urd::is_store<E>)
            elements.push_back(random_row<E>(random));
        else
            elements.push_back(std::uniform_int_distribution<int>(0, 9)(random));
    }
    return Row(elements.begin(), elements.end());
}

}  // namespace

TYPED_TEST(Rows, BehavesLikeAVectorOfVectors)
{
    using Row = typename TypeParam::value_type;
    for (unsigned seed = 0; seed < 64; ++seed)
    {
        auto random = std::mt19937(seed);
        auto store = TypeParam();
        auto model = materialize(store);
        for (int step = 0; step < 200; ++step)
        {
            const auto size = store.size();
            const int op = std::uniform_int_distribution<int>(0, 4)(random);
            if (step % 50 == 49)
            {
                store.clear();
                model.clear();
            }
            else if (op <= 1)
            {
                const auto row = random_row<Row>(random);
                store.push_back(row);
                model.push_back(materialize(row));
            }
            else if (op == 2)
            {
                const auto pos = std::uniform_int_distribution<std::size_t>(0, size)(random);
                const auto row = random_row<Row>(random);
                store.insert(store.begin() + static_cast<std::ptrdiff_t>(pos), row);
                model.insert(model.begin() + static_cast<std::ptrdiff_t>(pos), materialize(row));
            }
            else if (size > 0 && op == 3)
            {
                const auto pos = std::uniform_int_distribution<std::size_t>(0, size - 1)(random);
                store.erase(store.begin() + static_cast<std::ptrdiff_t>(pos));
                model.erase(model.begin() + static_cast<std::ptrdiff_t>(pos));
            }
            else if (size > 0)
            {
                store.pop_back();
                model.pop_back();
            }
            ASSERT_EQ(materialize(store), model) << "seed " << seed << ", step " << step;
        }
    }
}

TYPED_TEST(Rows, SortedRowsAreSorted)
{
    if constexpr (!urd::has<typename TypeParam::value_type, urd::sorted>)
        GTEST_SKIP() << "rows not sorted";
    else
    {
        auto random = std::mt19937(7);
        auto store = TypeParam();
        for (int count = 0; count < 50; ++count)
            store.push_back(random_row<typename TypeParam::value_type>(random));
        for (const auto& row : store)
        {
            const auto elements = materialize(row);
            ASSERT_TRUE(std::ranges::is_sorted(elements));
            for (int element = -1; element <= 10; ++element)
            {
                EXPECT_EQ(std::ranges::lower_bound(row, element) - row.begin(), std::ranges::lower_bound(elements, element) - elements.begin());
                EXPECT_EQ(std::ranges::binary_search(row, element), std::ranges::binary_search(elements, element));
            }
        }
    }
}

TYPED_TEST(Rows, RetainsAtEveryLevel)
{
    if (!urd::test::counts_allocations)
        GTEST_SKIP() << "allocations are not counted under ThreadSanitizer";
    if constexpr (!urd::has<TypeParam, urd::retains>)
        GTEST_SKIP() << "not asked to retain";
    else
    {
        // Rows of five elements, written in place where rows can grow, else appended whole.
        const auto elements = std::vector<int> { 4, 1, 3, 1, 5 };
        const auto fill = [&](TypeParam& store)
        {
            for (int count = 0; count < 100; ++count)
            {
                if constexpr (requires { store.emplace_back().push_back(1); })
                {
                    auto& row = store.emplace_back();
                    for (const int element : elements)
                        row.push_back(element);
                }
                else
                    store.emplace_back(std::span<const int>(elements));
            }
        };
        auto store = TypeParam();
        fill(store);
        const auto expected = materialize(store);
        store.clear();
        {
            const auto counted = Counted();
            fill(store);
        }
        EXPECT_EQ(allocations(), 0);
        EXPECT_EQ(materialize(store), expected);
    }
}
