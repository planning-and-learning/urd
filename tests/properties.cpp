// Every container offers the operations its declared properties require and keeps those properties. One typed
// test per property, written against the operations alone, runs for every container in the list below that
// declares the property, and is skipped for the others; one more drives all operations at random through
// urd::verify (verify.hpp). Validating a container is adding it to the list.

#include "urd/store.hpp"
#include "allocations.hpp"
#include "verify.hpp"

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <cstddef>
#include <iterator>
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

namespace lib
{

/// A library's append-only journal, with its own names. Entries never move once written.
class Journal
{
public:
    [[nodiscard]] auto begin() const { return m_entries.begin(); }
    [[nodiscard]] auto end() const { return m_entries.end(); }
    [[nodiscard]] std::size_t count() const { return m_entries.size(); }

    const int& append(int entry) { return m_entries.emplace_back(entry); }

private:
    std::deque<int> m_entries;
};

}  // namespace lib

namespace
{

/// An adapter: the library's journal under the names a growable sequence must offer. It only renames.
class Journal
{
public:
    Journal() = default;
    Journal(const int* first, const int* last)
    {
        for (; first != last; ++first)
            m_journal.append(*first);
    }

    [[nodiscard]] auto begin() const { return m_journal.begin(); }
    [[nodiscard]] auto end() const { return m_journal.end(); }
    [[nodiscard]] std::size_t size() const { return m_journal.count(); }

    void push_back(int entry) { m_journal.append(entry); }
    const int& emplace_back(int entry) { return m_journal.append(entry); }

private:
    lib::Journal m_journal;
};

}  // namespace

// What it guarantees: it only grows, and entries keep their address.
template<>
struct urd::container_traits<Journal> : urd::properties_of<urd::stable_refs, urd::growable>
{
};

namespace
{

using urd::key;
using urd::Store;
using urd::value;
using urd::repr::SegmentedVector;
using urd::test::allocations;
using urd::test::Counted;
using urd::test::deallocations;


using Containers = testing::Types<
    // The representations, as they are.
    std::vector<int>, std::set<int>, std::unordered_set<int>, boost::container::flat_set<int>, boost::container::flat_multiset<int>,
    SegmentedVector<int>, SegmentedVector<int, urd::repr::default_first_segment_size, true>,
    // A container from elsewhere, through an adapter.
    Journal,
    // Stores, which promise what they were asked for.
    Store<key<int>>, Store<key<int>, urd::sorted>, Store<key<int>, urd::small>, Store<key<int>, urd::sorted, urd::frozen>,
    Store<key<int>, urd::nonempty>, Store<key<int>, urd::stable_refs>, Store<key<int>, urd::sorted, urd::stable_refs>,
    Store<key<int>, urd::sorted, urd::contiguous, urd::frozen>, Store<value<int>>, Store<value<int>, urd::sorted>,
    Store<value<int>, urd::sorted, urd::unique>, Store<value<int>, urd::monotone>, Store<value<int>, urd::nonempty, urd::monotone>,
    Store<value<int>, urd::contiguous>, Store<value<int>, urd::stable_refs, urd::monotone>,
    Store<value<int>, urd::monotone, urd::concurrent>, Store<key<int>, urd::retains>, Store<key<int>, urd::sorted, urd::retains>,
    Store<value<int>, urd::retains>, Store<key<int>, urd::reclaims>,
    // Maps, as they are and as Stores.
    std::map<int, int>, std::multimap<int, int>, std::unordered_map<int, int>, std::unordered_multimap<int, int>,
    boost::container::flat_map<int, int>, boost::container::flat_multimap<int, int>, Store<key<int>, value<int>>,
    Store<key<int>, value<int>, urd::sorted>, Store<key<int>, value<int>, urd::small>, Store<key<int>, value<int>, urd::sorted, urd::frozen>,
    Store<key<int>, value<int>, urd::multi>, Store<key<int>, value<int>, urd::multi, urd::sorted>,
    Store<key<int>, value<int>, urd::stable_refs>, Store<key<int>, value<int>, urd::retains>>;

template<typename R>
class Properties : public testing::Test
{
};
TYPED_TEST_SUITE(Properties, Containers);

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

template<typename R>
R built(const std::vector<int>& keys)
{
    if constexpr (map<R>)
    {
        auto pairs = std::vector<std::pair<int, int>>();
        for (const int key : keys)
            pairs.emplace_back(key, -key);
        return R(pairs.begin(), pairs.end());
    }
    else
        return R(keys.data(), keys.data() + keys.size());
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


TYPED_TEST(Properties, OffersWhatItsPropertiesRequire) { static_assert(urd::conforming<TypeParam>); }

TYPED_TEST(Properties, KeepsItsPropertiesThroughRandomOperations)
{
    // Many runs, each starting fresh: most faults show at small sizes (empty, first growth, last removal). A step
    // checks the whole container, so a container that only grows costs O(steps²) a run and gets short ones;
    // one that shrinks stays small and gets long ones, with deeper sequences of erases. Large sizes are the
    // per-property tests' job.
    constexpr int steps = declares<TypeParam, urd::shrinkable> ? 1000 : 200;
    for (unsigned seed = 0; seed < 64; ++seed)
    {
        const auto found = urd::verify(built<TypeParam>({ 5, 3, 3, 9, 1 }), seed, steps);
        EXPECT_FALSE(found) << *found;
    }
}

TYPED_TEST(Properties, Sorted)
{
    if constexpr (!declares<TypeParam, urd::sorted>)
        GTEST_SKIP() << "not declared";
    else
    {
        auto range = built<TypeParam>(descending(100));
        if constexpr (growable<TypeParam>)
            for (const int element : descending(300))
                add(range, element);
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

TYPED_TEST(Properties, Unique)
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

TYPED_TEST(Properties, StableRefs)
{
    if constexpr (!declares<TypeParam, urd::stable_refs> || !growable<TypeParam>)
        GTEST_SKIP() << "not declared, or nothing changes it";
    else
    {
        auto range = built<TypeParam>({ -1 });
        const auto* first = std::addressof(*std::ranges::begin(range));
        for (int element = 0; element < 100000; ++element)
            add(range, element);
        const auto found = std::ranges::find(range, -1, key_of);
        ASSERT_NE(found, std::ranges::end(range));
        EXPECT_EQ(std::addressof(*found), first);
    }
}

TYPED_TEST(Properties, Contiguous)
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
        auto pos = std::size_t { 0 };
        for (const auto& element : range)
            EXPECT_EQ(std::addressof(element), data + pos++);
    }
}

TYPED_TEST(Properties, Canonical)
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

TYPED_TEST(Properties, Nonempty)
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

TYPED_TEST(Properties, Retains)
{
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
        EXPECT_EQ(allocations, 0) << "after clear";

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
        EXPECT_EQ(allocations, 0) << "after erasing each element";
    }
}

TYPED_TEST(Properties, Reclaims)
{
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
        EXPECT_GE(deallocations, COUNT);
    }
}

TYPED_TEST(Properties, Concurrent)
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
            threads.emplace_back([&range, thread] {
                for (int element = 0; element < EACH; ++element)
                    add(range, thread * EACH + element);
            });
        for (auto& thread : threads)
            thread.join();
        auto contents = std::vector<int>(std::ranges::begin(range), std::ranges::end(range));
        std::ranges::sort(contents);
        auto expected = std::vector<int>(THREADS * EACH);
        std::iota(expected.begin(), expected.end(), 0);
        EXPECT_EQ(contents, expected);
    }
}

namespace
{

/// A vector that wrongly claims its elements never move.
struct ClaimsStableRefs
{
    std::vector<int> elements;

    ClaimsStableRefs() = default;
    ClaimsStableRefs(const int* first, const int* last) : elements(first, last) {}
    [[nodiscard]] auto begin() const { return elements.begin(); }
    [[nodiscard]] auto end() const { return elements.end(); }
    [[nodiscard]] std::size_t size() const { return elements.size(); }
    void push_back(int element) { elements.push_back(element); }
    const int& emplace_back(int element) { return elements.emplace_back(element); }
};

}  // namespace

template<>
struct urd::container_traits<ClaimsStableRefs> : urd::properties_of<urd::stable_refs, urd::growable>
{
};

TEST(Properties, AFalseClaimIsCaught)
{
    const auto found = urd::verify(ClaimsStableRefs(), 31);
    ASSERT_TRUE(found);
    EXPECT_NE(found->find("stable_refs"), std::string::npos) << *found;
}
