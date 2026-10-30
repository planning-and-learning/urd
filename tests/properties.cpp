// Every container offers the operations its declared properties require and keeps those properties. One typed
// test per property, written against the operations alone, runs for every container in the list below that
// declares the property, and is skipped for the others; one more drives all operations at random through
// urd::verify (verify.hpp). Validating a container is adding it to the list.

#include "allocations.hpp"
#include "names.hpp"
#include "urd/repr/dense.hpp"
#include "urd/store.hpp"
#include "urd/use/contiguous.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/layers.hpp"
#include "urd/use/nodes.hpp"
#if defined(URD_HAS_GTL)
#include "urd/use/parallel.hpp"
#endif
#include "urd/use/scopes.hpp"
#include "urd/use/sequences.hpp"
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
}  // namespace

namespace
{

/// An adapter: the library's journal under the names a growable sequence must offer. It only renames.
class Journal
{
public:
    Journal() = default;
    template<std::input_iterator InputIt>
    Journal(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](int entry) { m_journal.append(entry); });
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
struct urd::container_traits<Journal> : urd::properties_of<urd::positional, urd::stable_refs, urd::growable>
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

/// Orders a map by its values, ties by key: what order_by<P> with P(key, value) asks for.
constexpr auto by_value = [](int /*key*/, int value) { return value; };

using Containers = testing::Types<
    // The representations, as they are.
    std::vector<int>,
    std::set<int>,
    std::unordered_set<int>,
    boost::container::flat_set<int>,
    boost::container::flat_multiset<int>,
    SegmentedVector<int>,
    SegmentedVector<int, urd::repr::default_first_segment_size, true>,
    // A container from elsewhere, through an adapter.
    Journal,
    // Stores, which promise what they were asked for.
    Store<key<int>>,
    Store<key<int>, urd::sorted>,
    Store<key<int>, urd::small>,
    Store<key<int>, urd::sorted, urd::frozen>,
    Store<key<int>, urd::nonempty>,
    Store<key<int>, urd::stable_refs>,
    Store<key<int>, urd::sorted, urd::stable_refs>,
    Store<key<int>, urd::sorted, urd::contiguous, urd::frozen>,
    Store<value<int>>,
    Store<value<int>, urd::sorted>,
    Store<value<int>, urd::sorted, urd::unique>,
    Store<value<int>, urd::monotone>,
    Store<value<int>, urd::nonempty, urd::monotone>,
    Store<value<int>, urd::contiguous>,
    Store<value<int>, urd::stable_refs, urd::monotone>,
    Store<value<int>, urd::monotone, urd::concurrent>,
    Store<key<int>, urd::retains>,
    Store<key<int>, urd::sorted, urd::retains>,
    Store<value<int>, urd::retains>,
    Store<key<int>, urd::reclaims>,
    // Maps, as they are and as Stores.
    std::map<int, int>,
    std::multimap<int, int>,
    std::unordered_map<int, int>,
    std::unordered_multimap<int, int>,
    boost::container::flat_map<int, int>,
    boost::container::flat_multimap<int, int>,
    boost::unordered_flat_set<int, std::hash<int>>,
    boost::unordered_flat_map<int, int, std::hash<int>>,
    Store<key<int>, value<int>>,
    Store<key<int>, value<int>, urd::sorted>,
    Store<key<int>, value<int>, urd::small>,
    Store<key<int>, value<int>, urd::sorted, urd::frozen>,
    Store<key<int>, value<int>, urd::multi>,
    Store<key<int>, value<int>, urd::multi, urd::sorted>,
    Store<key<int>, value<int>, urd::stable_refs>,
    Store<key<int>, value<int>, urd::retains>,
    // Dense keys: indices, by position.
    urd::repr::DenseSet<int>,
    urd::repr::SparseSet<int>,
    Store<key<int>, urd::dense>,
    Store<key<int>, urd::dense, urd::sorted>,
    Store<key<int>, urd::dense, urd::constant_clear>,
    urd::repr::DenseSet<int, true>,
    Store<key<int>, urd::dense, urd::proportional_clear>,
    urd::repr::SparseSet<int, true>,
    Store<key<int>, urd::dense, urd::insertion_ordered>,
    urd::repr::IndexMap<int, int, false>,
    urd::repr::IndexMap<int, int, false, false, true>,
    urd::repr::IndexMap<int, int, true>,
    urd::repr::IndexMap<int, int, true, false, true>,
    urd::repr::IndexMap<int, int, true, true>,
    Store<key<int>, value<int>, urd::dense, urd::constant_clear, urd::concurrent>,
    Store<key<int>, value<int>, urd::dense>,
    Store<key<int>, value<int>, urd::dense, urd::stable_refs>,
    Store<key<int>, value<int>, urd::dense, urd::constant_clear>,
    Store<key<int>, value<int>, urd::dense, urd::constant_clear, urd::stable_refs>,
    // Maps in insertion order: keys in an insertion-ordered set, values beside them.
    urd::repr::OrderedMap<urd::repr::IndexedSet<int>, int>,
    urd::repr::OrderedMap<urd::repr::SparseSet<int, true>, int>,
    Store<key<int>, value<int>, urd::insertion_ordered>,
    Store<key<int>, value<int>, urd::dense, urd::insertion_ordered>,
    // Insertion order and positions: interners.
    urd::repr::IndexedSet<int>,
    Store<key<int>, urd::insertion_ordered>,
    Store<key<int>, urd::positional>,
    Store<key<int>, urd::positional, urd::monotone>,
    urd::repr::StableIndexedSet<int>,
    Store<key<int>, urd::positional, urd::monotone, urd::stable_refs>,
    urd::repr::ConcurrentIndexedSet<int>,
    Store<key<int>, urd::positional, urd::monotone, urd::concurrent>,
    // Frozen multimaps by dense key: compressed sparse rows.
    urd::repr::CsrMultiMap<int, int>,
    urd::repr::DenseMultiMap<int, int>,
    Store<key<int>, value<int>, urd::multi, urd::dense>,
    urd::repr::BidirectionalCsr<int, int>,
    Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen>,
    // Postings: each key's values together in a hash table.
    urd::repr::PostingMap<int, int>,
    Store<key<int>, value<int>, urd::multi, urd::stable_refs>,
    // Scopes and layers.
    urd::repr::ScopedMap<int, int>,
    urd::repr::ScopedTreeMap<int, int>,
    urd::repr::ScopedDenseMap<int, int>,
    Store<key<int>, value<int>, urd::scoped>,
    Store<key<int>, value<int>, urd::sorted, urd::scoped>,
    Store<key<int>, value<int>, urd::dense, urd::scoped>,
    urd::repr::Layered<boost::unordered_flat_set<int, std::hash<int>>>,
    urd::repr::Layered<boost::unordered_flat_map<int, int, std::hash<int>>>,
    urd::repr::Layered<std::unordered_set<int, urd::Hash<int>, urd::EqualTo<int>>>,
    urd::repr::Layered<std::unordered_map<int, int, urd::Hash<int>, urd::EqualTo<int>>>,
    urd::repr::Layered<urd::repr::IndexMap<int, int, true>>,
    Store<key<int>, urd::layered>,
    Store<key<int>, value<int>, urd::layered>,
    Store<key<int>, urd::stable_refs, urd::layered>,
    Store<key<int>, value<int>, urd::stable_refs, urd::layered>,
    Store<key<int>, value<int>, urd::dense, urd::constant_clear, urd::layered>,
    // Change logs.
    urd::repr::ChangeLogged<boost::unordered_flat_set<int, std::hash<int>>>,
    urd::repr::ChangeLogged<urd::repr::DenseSet<int, true>>,
    urd::repr::ChangeLogged<std::set<int, urd::Less<int>>>,
    urd::repr::ChangeLogged<std::unordered_set<int, urd::Hash<int>, urd::EqualTo<int>>>,
    Store<key<int>, urd::change_log>,
    Store<key<int>, urd::dense, urd::change_log>,
    Store<key<int>, urd::sorted, urd::change_log>,
    Store<key<int>, urd::stable_refs, urd::change_log>,
    // Maps ordered by their values: entries by P(key, value), found by key.
    urd::repr::FlatEntryMap<int, int, urd::EntryLess<int, int, by_value>>,
    Store<key<int>, value<int>, urd::sorted, urd::small, urd::order_by<by_value>>,
    // A sorted map that keeps erased nodes and their values for reuse.
    urd::repr::RecyclingMap<int, int>,
    urd::repr::RecyclingHashMap<int, int>,
    // Sets that keep erased nodes for reuse.
    urd::repr::RecyclingHashSet<int>,
    urd::repr::RecyclingTreeSet<int>,
    Store<key<int>, urd::stable_refs, urd::retains>,
    Store<key<int>, urd::sorted, urd::stable_refs, urd::retains>
#if defined(URD_HAS_GTL)
    ,
    // Sharded hash tables that threads share (gtl).
    urd::repr::ParallelHashSet<int>,
    urd::repr::ParallelHashMap<int, int>,
    urd::repr::ParallelNodeHashSet<int>,
    urd::repr::ParallelNodeHashMap<int, int>,
    Store<key<int>, urd::concurrent>,
    Store<key<int>, value<int>, urd::concurrent>,
    Store<key<int>, urd::stable_refs, urd::concurrent>,
    Store<key<int>, value<int>, urd::stable_refs, urd::concurrent>
#endif
    >;

template<typename R>
class Properties : public testing::Test
{
};
TYPED_TEST_SUITE(Properties, Containers, urd::test::Names);

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
        // Values differ from keys; a bidirectional map looks them up, so they are indices there.
        for (const int key : keys)
            pairs.emplace_back(key, declares<R, urd::bidirectional> ? 10 * key : -key);
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

TYPED_TEST(Properties, OffersWhatItsPropertiesRequire) { static_assert(urd::conforming<TypeParam>); }

namespace
{
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
}  // namespace

// A container declares positional exactly where it is a random-access range: selection reads the declaration, since
// it may not look inside a candidate (the container need not be defined where a request is resolved). And every
// Store is movable, which a generation map's slots rely on without asking the Store (traits.hpp, slot_value).
TYPED_TEST(Properties, DeclaresPositionalWhereRandomAccess)
{
    using R = typename representation_of<TypeParam>::type;
    static_assert(declares<R, urd::positional> == std::ranges::random_access_range<R>);
    static_assert(std::movable<TypeParam>);
}

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
        EXPECT_FALSE(found) << found.value_or("");
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
        auto pos = std::ptrdiff_t { 0 };
        for (const auto& element : range)
            EXPECT_EQ(std::addressof(element), std::next(data, pos++));
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

TYPED_TEST(Properties, Reclaims)
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

TYPED_TEST(Properties, ClearCost)
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
TYPED_TEST(Properties, ClearCostInLiveElements)
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

namespace
{

/// A vector that wrongly claims its elements never move.
struct ClaimsStableRefs
{
    std::vector<int> elements;

    ClaimsStableRefs() = default;
    template<std::input_iterator InputIt>
    ClaimsStableRefs(InputIt first, InputIt last) : elements(first, last)
    {
    }
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
    const auto found = urd::verify(ClaimsStableRefs(), 31).value_or("");
    EXPECT_NE(found.find("stable_refs"), std::string::npos) << found;
}

namespace
{

/// A dense map whose slots lie in a vector, wrongly claiming its values never move. Its iteration builds each
/// entry on the fly, so only a check through the values (not the entries) catches it.
struct ClaimsStableValues : urd::repr::IndexMap<int, int, false>
{
    using IndexMap::IndexMap;
};

}  // namespace

template<>
struct urd::container_traits<ClaimsStableValues> : urd::properties_of<urd::sorted, urd::unique, urd::dense, urd::stable_refs, urd::shrinkable, urd::retains>
{
};

TEST(Properties, AFalseClaimAboutValuesIsCaught)
{
    const auto found = urd::verify(ClaimsStableValues(), 31).value_or("");
    EXPECT_NE(found.find("stable_refs"), std::string::npos) << found;
}
