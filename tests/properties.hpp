#pragma once

// The properties a container declares, each checked by a test written against the operations alone: one typed test
// per property, skipped for a container that does not declare it, and one that drives every operation at random
// through urd::verify. A type-parameterized suite (GoogleTest's TYPED_TEST_SUITE_P): each container's test object
// (tests/container.cpp, compiled once per container) instantiates it for that container and the Stores over it, so
// adding a container adds its own tests. The headers a container needs are included before this header.

#include "allocations.hpp"
#include "names.hpp"
#include "urd/store.hpp"
#include "verify.hpp"

#include <algorithm>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <chrono>
#include <cstddef>
#include <deque>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <ranges>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

using urd::key;
using urd::Store;
using urd::value;
using urd::test::allocations;
using urd::test::Counted;
using urd::test::deallocations;

/// Orders a map by its values, ties by key: what order_by<P> with P(key, value) asks for.
constexpr auto by_value = [](int /*key*/, int value) { return value; };

template<typename R>
class Properties : public testing::Test
{
};
TYPED_TEST_SUITE_P(Properties);

template<typename R, typename Tag>
constexpr bool declares = urd::has<urd::container_traits<R>, Tag>;

template<typename R>
constexpr bool growable = declares<R, urd::growable>;

template<typename R>
constexpr bool map = urd::detail::map_container<R>;

/// R's element for `key`: the key itself, or in a map the pair (key, -key).
template<typename R>
auto element(int key)
{
    if constexpr (map<R>)
        return std::pair<const int, int>(key, -key);
    else
        return key;
}

/// What an element is ordered and looked up by: its key in a map, else itself.
constexpr auto key_of = [](const auto& element)
{
    if constexpr (requires { element.first; })
        return element.first;
    else
        return element;
};

/// The representation a type is: a Store's, or the type itself.
template<typename T>
struct representation_of
{
    using type = T;
};
template<typename R, typename S>
struct representation_of<urd::detail::Store<R, S>>
{
    using type = R;
};

template<typename R>
R built(const std::vector<int>& keys)
{
    if constexpr (map<R>)
    {
        auto pairs = std::vector<std::pair<int, int>>();
        // Values differ from keys; a bidirectional map looks them up, so they are indices there (the representation
        // decides: a Store over one may not have asked for bidirectional).
        for (const int key : keys)
            pairs.emplace_back(key, declares<typename representation_of<R>::type, urd::bidirectional> ? 10 * key : -key);
        return R(pairs.begin(), pairs.end());
    }
    else
        return R(keys.begin(), keys.end());
}

/// Adds the element for `key` as R's properties say: by value, or at the end.
template<typename R>
void add(R& range, int key)
{
    if constexpr (urd::by_value<urd::container_traits<R>> || map<R>)
        range.insert(element<R>(key));
    else
        range.push_back(key);
}

std::vector<int> descending(int count)
{
    auto result = std::vector<int>();
    for (int element = count - 1; element >= 0; --element)
        result.push_back(element);
    return result;
}

}  // namespace

TYPED_TEST_P(Properties, OffersWhatItsPropertiesRequire) { static_assert(urd::conforming<TypeParam>); }

// A container declares positional exactly where it is a random-access range: selection reads the declaration, since
// it may not look inside a candidate (the container need not be defined where a request is resolved). And every
// Store is movable, which a generation map's slots rely on without asking the Store (traits.hpp, slot_value).
TYPED_TEST_P(Properties, DeclaresPositionalWhereRandomAccess)
{
    using R = typename representation_of<TypeParam>::type;
    static_assert(declares<R, urd::positional> == std::ranges::random_access_range<R>);
    static_assert(std::movable<TypeParam>);
}

TYPED_TEST_P(Properties, KeepsItsPropertiesThroughRandomOperations)
{
    // Many runs, each starting fresh: most faults show at small sizes (empty, first growth, last removal). A step
    // checks the whole container, so a container that only grows costs O(steps²) a run and gets short ones;
    // one that shrinks stays small and gets long ones, with deeper sequences of erases. Large sizes are the
    // per-property tests' job.
    constexpr int steps = declares<TypeParam, urd::shrinkable> ? 1000 : 200;
    for (unsigned seed = 0; seed < 64; ++seed)
    {
        const auto found = urd::verify(built<TypeParam>({ 5, 3, 3, 9, 1 }), seed, steps);
        EXPECT_FALSE(found) << found.value_or("");
    }
}

TYPED_TEST_P(Properties, Sorted)
{
    if constexpr (!declares<TypeParam, urd::sorted>)
        GTEST_SKIP() << "not declared";
    else
    {
        auto range = built<TypeParam>(descending(100));
        if constexpr (growable<TypeParam>)
            for (const int element : descending(300))
                add(range, element);
        if constexpr (urd::orders_by_value<TypeParam>)
            // In the order the request gave, which reads the values; there are no bounds by key.
            EXPECT_TRUE(std::ranges::is_sorted(range, urd::detail::value_order_t<TypeParam> {}));
        else
        {
            EXPECT_TRUE(std::ranges::is_sorted(range, {}, key_of));
            for (const int element : { -1, 0, 50, 299, 300 })
            {
                const auto [first, last] = std::as_const(range).equal_range(element);
                EXPECT_EQ(first, std::as_const(range).lower_bound(element));
                EXPECT_EQ(last, std::as_const(range).upper_bound(element));
                EXPECT_EQ(first, std::ranges::lower_bound(std::as_const(range), element, {}, key_of));
                EXPECT_EQ(last, std::ranges::upper_bound(std::as_const(range), element, {}, key_of));
            }
        }
    }
}

TYPED_TEST_P(Properties, Unique)
{
    if constexpr (!declares<TypeParam, urd::unique>)
        GTEST_SKIP() << "not declared";
    else
    {
        auto range = built<TypeParam>({ 3, 1, 3, 3 });
        EXPECT_EQ(std::ranges::size(range), 2U);
        EXPECT_EQ(std::as_const(range).count(3), 1U);
        if constexpr (growable<TypeParam>)
        {
            EXPECT_FALSE(range.insert(element<TypeParam>(1)).second);
            EXPECT_TRUE(range.insert(element<TypeParam>(2)).second);
            EXPECT_EQ(std::ranges::size(range), 3U);
        }
    }
}

TYPED_TEST_P(Properties, StableRefs)
{
    if constexpr (!declares<TypeParam, urd::stable_refs> || !growable<TypeParam>)
        GTEST_SKIP() << "not declared, or nothing changes it";
    else
    {
        // The reference is taken by the path the kind hands it out: a map's value by at (by find for a multimap), a
        // sequence's element by position, a set's by find; through iteration where there is nothing else.
        static constexpr int probe = declares<TypeParam, urd::dense> ? 0 : -1;
        auto range = built<TypeParam>({ probe });
        const auto reference = [&range]() -> const void*
        {
            if constexpr (map<TypeParam> && requires { std::as_const(range).at(probe); })
                return std::addressof(std::as_const(range).at(probe));
            else if constexpr (map<TypeParam>)
                return std::addressof(std::as_const(range).find(probe)->second);
            else if constexpr (requires { std::as_const(range)[0]; })
                return std::addressof(std::as_const(range)[0]);
            else if constexpr (requires { std::as_const(range).find(probe); })
                return std::addressof(*std::as_const(range).find(probe));
            else
                return std::addressof(*std::ranges::begin(range));
        };
        const auto* first = reference();
        for (int element = 1; element <= 100000; ++element)
            add(range, element);
        EXPECT_EQ(reference(), first);
    }
}

TYPED_TEST_P(Properties, Contiguous)
{
    if constexpr (!declares<TypeParam, urd::contiguous>)
        GTEST_SKIP() << "not declared";
    else
    {
        auto range = built<TypeParam>(descending(1000));
        if constexpr (growable<TypeParam>)
            for (int element = 1000; element < 3000; ++element)
                add(range, element);
        const auto* data = std::ranges::data(range);
        auto pos = std::ptrdiff_t { 0 };
        for (const auto& element : range)
            EXPECT_EQ(std::addressof(element), std::next(data, pos++));
    }
}

TYPED_TEST_P(Properties, Canonical)
{
    if constexpr (!declares<TypeParam, urd::canonical>)
        GTEST_SKIP() << "not declared";
    else
    {
        auto elements = descending(50);
        const auto reversed = built<TypeParam>(elements);
        std::ranges::reverse(elements);
        EXPECT_TRUE(built<TypeParam>(elements) == reversed);
    }
}

TYPED_TEST_P(Properties, Nonempty)
{
    if constexpr (!declares<TypeParam, urd::nonempty>)
        GTEST_SKIP() << "not declared";
    else
    {
        // Nothing removes its elements, and it cannot start empty.
        static_assert(!declares<TypeParam, urd::shrinkable> && !std::default_initializable<TypeParam>);
        EXPECT_FALSE(std::ranges::empty(built<TypeParam>({ 1 })));
    }
}

TYPED_TEST_P(Properties, Retains)
{
    if (!urd::test::counts_allocations)
        GTEST_SKIP() << "allocations are not counted under ThreadSanitizer";
    if constexpr (!declares<TypeParam, urd::retains> || !declares<TypeParam, urd::shrinkable>)
        GTEST_SKIP() << "not declared, or nothing is removed";
    else
    {
        constexpr int COUNT = 1000;
        auto range = TypeParam();
        for (int element = 0; element < COUNT; ++element)
            add(range, element);

        // Cleared, then refilled: nothing is allocated.
        range.clear();
        {
            const auto counted = Counted();
            for (int element = 0; element < COUNT; ++element)
                add(range, element);
        }
        EXPECT_EQ(allocations(), 0) << "after clear";

        // Erased one by one, then refilled: nothing is allocated either.
        for (int element = 0; element < COUNT; ++element)
        {
            if constexpr (urd::by_value<urd::container_traits<TypeParam>>)
                range.erase(element);
            else
                range.pop_back();
        }
        {
            const auto counted = Counted();
            for (int element = 0; element < COUNT; ++element)
                add(range, element);
        }
        EXPECT_EQ(allocations(), 0) << "after erasing each element";
    }
}

TYPED_TEST_P(Properties, Reclaims)
{
    if (!urd::test::counts_allocations)
        GTEST_SKIP() << "allocations are not counted under ThreadSanitizer";
    if constexpr (!declares<TypeParam, urd::reclaims>)
        GTEST_SKIP() << "not declared";
    else
    {
        // Clearing frees the storage of every element.
        constexpr int COUNT = 1000;
        auto range = TypeParam();
        for (int element = 0; element < COUNT; ++element)
            add(range, element);
        {
            const auto counted = Counted();
            range.clear();
        }
        EXPECT_GE(deallocations(), COUNT);
    }
}

namespace
{

/// For ClearCost: a key far beyond the 8 present, and how many timings to take the shortest of.
constexpr int far_key = 1 << 22;
constexpr int timing_runs = 5;

/// The shortest of `runs` timings of `measured`, in nanoseconds; `prepare` runs before each, untimed.
template<typename Prepare, typename Measured>
std::int64_t shortest(int runs, Prepare prepare, Measured measured)
{
    auto best = std::numeric_limits<std::int64_t>::max();
    for (int run = 0; run < runs; ++run)
    {
        auto state = prepare();
        const auto start = std::chrono::steady_clock::now();
        measured(state);
        const auto stop = std::chrono::steady_clock::now();
        best = std::min(best, static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count()));
    }
    return best;
}

}  // namespace

TYPED_TEST_P(Properties, ClearCost)
{
    if constexpr (!declares<TypeParam, urd::proportional_clear> || !declares<TypeParam, urd::shrinkable>)
        GTEST_SKIP() << "no clear cost declared";
    else
    {
        // 8 keys present, after a history that did or did not reach a large key: clearing must not cost what the
        // history did. A clear that scans the key range takes far longer after the far key.
        const auto clear_after = [](bool far)
        {
            return shortest(
                timing_runs,
                [far]
                {
                    auto range = TypeParam();
                    if (far)
                    {
                        add(range, far_key);
                        if constexpr (urd::by_value<urd::container_traits<TypeParam>> || map<TypeParam>)
                            range.erase(far_key);
                        else
                            range.pop_back();
                    }
                    for (int element = 0; element < 8; ++element)
                        add(range, element);
                    return range;
                },
                [](TypeParam& range) { range.clear(); });
        };
        const auto near_time = clear_after(false);
        const auto far_time = clear_after(true);
        constexpr std::int64_t slack = 1000;  // nanoseconds, for the clock
        EXPECT_LT(far_time, 10 * near_time + slack) << "clear took " << near_time << " ns, after a far key " << far_time << " ns";
    }
}

// A constant clear costs the same with many live elements as with few: a generation counter or a size reset, not
// a pass over what is present, which proportional_clear allows and constant_clear rules out.
TYPED_TEST_P(Properties, ClearCostInLiveElements)
{
    if constexpr (!declares<TypeParam, urd::constant_clear> || !declares<TypeParam, urd::shrinkable>)
        GTEST_SKIP() << "no constant clear declared";
    else
    {
        const auto clear_with = [](int live)
        {
            return shortest(
                timing_runs,
                [live]
                {
                    auto range = TypeParam();
                    // The same history for both: a far key touched and erased, then the live elements.
                    add(range, far_key);
                    if constexpr (urd::by_value<urd::container_traits<TypeParam>> || map<TypeParam>)
                        range.erase(far_key);
                    else
                        range.pop_back();
                    for (int element = 0; element < live; ++element)
                        add(range, element);
                    return range;
                },
                [](TypeParam& range) { range.clear(); });
        };
        const auto few = clear_with(8);
        const auto many = clear_with(8192);
        constexpr std::int64_t slack = 2000;  // nanoseconds, for the clock
        EXPECT_LT(many, 10 * few + slack) << "clear took " << few << " ns with 8 elements, " << many << " ns with 8192";
    }
}

TYPED_TEST_P(Properties, Concurrent)
{
    if constexpr (!declares<TypeParam, urd::concurrent>)
        GTEST_SKIP() << "not declared";
    else
    {
        // Inserts from several threads all arrive (run under ThreadSanitizer to see races).
        constexpr int THREADS = 4;
        constexpr int EACH = 20000;
        auto range = TypeParam();
        auto threads = std::vector<std::thread>();
        for (int thread = 0; thread < THREADS; ++thread)
            threads.emplace_back(
                [&range, thread]
                {
                    for (int element = 0; element < EACH; ++element)
                        add(range, thread * EACH + element);
                });
        for (auto& thread : threads)
            thread.join();
        auto contents = std::vector<int>();
        for (const auto& element : range)
        {
            if constexpr (map<TypeParam>)
                contents.push_back(element.first);
            else
                contents.push_back(element);
        }
        std::ranges::sort(contents);
        auto expected = std::vector<int>(static_cast<std::size_t>(THREADS) * EACH);
        std::iota(expected.begin(), expected.end(), 0);
        EXPECT_EQ(contents, expected);
    }
}

REGISTER_TYPED_TEST_SUITE_P(Properties,
                            OffersWhatItsPropertiesRequire,
                            DeclaresPositionalWhereRandomAccess,
                            KeepsItsPropertiesThroughRandomOperations,
                            Sorted,
                            Unique,
                            StableRefs,
                            Contiguous,
                            Canonical,
                            Nonempty,
                            Retains,
                            Reclaims,
                            ClearCost,
                            ClearCostInLiveElements,
                            Concurrent);
