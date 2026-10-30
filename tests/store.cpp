// A Store resolves the same request to the same type in any order, to the representation its properties and
// workload select, offers what was asked for, and behaves like the reference container under random use.

// Every family: this file checks that every registered candidate offers what its traits declare.
#include "urd/store.hpp"

#include "urd/use/contiguous.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/layers.hpp"
#include "urd/use/nodes.hpp"
#include "urd/use/pools.hpp"
#include "urd/use/queues.hpp"
#include "urd/use/records.hpp"
#include "urd/use/scopes.hpp"
#include "urd/use/sequences.hpp"
#if defined(URD_HAS_GTL)
#include "urd/use/parallel.hpp"
#endif

#include "allocations.hpp"
#include "urd/compose/family.hpp"
#include "urd/concepts.hpp"

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

// The order of the properties does not matter.
static_assert(std::same_as<Store<key<int>, urd::sorted>, Store<urd::sorted, key<int>>>);
static_assert(std::same_as<Store<value<int>, urd::unique, urd::sorted>, Store<urd::sorted, value<int>, urd::unique>>);

// Implied properties: a set's keys are unique, values without keys are a sequence, frozen keeps references.
static_assert(urd::has<Store<key<int>>, urd::unique>);
static_assert(urd::has<Store<value<int>>, urd::positional, urd::functional, urd::dense>);
static_assert(urd::has<Store<key<int>, urd::frozen>, urd::stable_refs>);

// What each request resolves to (pinned, so a change in the selection shows up here). Where no order is asked
// for, an open-addressing hash table, which docs/BENCHMARKS.md finds fastest at every size.
// The registry's names: each container with urd's hash, equality and order.
using urd::detail::FlatHashMapOf;
using urd::detail::FlatHashSet;
using urd::detail::FlatMapOf;
using urd::detail::FlatMultiSet;
using urd::detail::FlatSet;
using urd::detail::HashMapOf;
using urd::detail::HashMultiMapOf;
using urd::detail::HashSet;
using urd::detail::TreeMapOf;
using urd::detail::TreeMultiMapOf;
using urd::detail::TreeSet;
static_assert(std::same_as<Store<key<int>>::representation, FlatHashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted>::representation, TreeSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::frozen>::representation, FlatSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::small>::representation, FlatHashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::stable_refs>::representation, HashSet<int>>);
// Few elements: contiguous storage first, node-based containers last, a hash table before a tree among them.
static_assert(std::same_as<Store<key<int>, urd::small, urd::stable_refs>::representation, HashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::small, urd::sorted, urd::stable_refs>::representation, TreeSet<int>>);
static_assert(std::same_as<Store<value<int>>::representation, std::vector<int>>);
static_assert(std::same_as<Store<value<int>, urd::sorted>::representation, FlatMultiSet<int>>);
// Elements that must not move: segments, unless nothing changes anyway.
static_assert(std::same_as<Store<value<int>, urd::stable_refs, urd::monotone>::representation, urd::repr::SegmentedVector<int>>);
static_assert(std::same_as<Store<value<int>, urd::monotone, urd::concurrent>::representation,
                           urd::repr::SegmentedVector<int, urd::repr::default_first_segment_size, true>>);
static_assert(std::same_as<Store<value<int>, urd::stable_refs, urd::frozen>::representation, std::vector<int>>);
// Maps: by key, as the sets are by element.
static_assert(std::same_as<Store<key<int>, value<int>>::representation, FlatHashMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::sorted>::representation, TreeMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::small>::representation, FlatHashMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::sorted, urd::frozen>::representation, FlatMapOf<int, int>>);
// A multimap is postings: a key's values together, one probe away. Stable references need the node-based table.
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi>::representation, urd::repr::PostingMap<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi, urd::stable_refs>::representation, HashMultiMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi, urd::sorted>::representation, TreeMultiMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::stable_refs>::representation, HashMapOf<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::retains>::representation, FlatHashMapOf<int, int>>);
// A map of rows holds the inner Stores; flattening by dense key comes with dense maps.
static_assert(std::same_as<Store<key<int>, value<Store<value<int>, urd::frozen>>>::representation, FlatHashMapOf<int, Store<value<int>, urd::frozen>>>);
// A map's values change by key, never through its iterators; only a map with unique keys has at and try_emplace.
template<typename S>
concept can_assign_by_key = requires(S store) { store[1] = 2; };
template<typename S>
concept can_try_emplace = requires(S store) { store.try_emplace(1, 2); };
static_assert(can_assign_by_key<Store<key<int>, value<int>>> && !can_assign_by_key<Store<key<int>, value<int>, urd::frozen>>);
static_assert(can_try_emplace<Store<key<int>, value<int>>> && !can_try_emplace<Store<key<int>, value<int>, urd::multi>>);
static_assert(std::same_as<decltype(*Store<key<int>, value<int>>().begin()), const std::pair<const int, int>&>);

// Dense keys (indices): stored by position, a bitset where nothing else is asked, a sparse set where clear must be
// O(1).
static_assert(std::same_as<Store<key<int>, urd::dense>::representation, urd::repr::DenseSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::dense, urd::sorted>::representation, urd::repr::DenseSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::dense, urd::constant_clear>::representation, urd::repr::SparseSet<int>>);
// A bitset that clears only the words it set: sorted, cleared in time proportional to its elements.
static_assert(std::same_as<Store<key<int>, urd::dense, urd::proportional_clear>::representation, urd::repr::DenseSet<int, true>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::dense>::representation, urd::repr::IndexMap<int, int, false>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::dense, urd::constant_clear>::representation, urd::repr::IndexMap<int, int, true>>);
// Values that never move: the slots in segments, without the lock the thread-safe map pays.
static_assert(std::same_as<Store<key<int>, value<int>, urd::dense, urd::stable_refs>::representation, urd::repr::IndexMap<int, int, false, false, true>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::dense, urd::constant_clear, urd::stable_refs>::representation,
                           urd::repr::IndexMap<int, int, true, false, true>>);

// Insertion order and positions: an indexed set. A set that is positional keeps insertion order unless it asks to be
// sorted, so that an element's position is its index (an interner).
static_assert(std::same_as<Store<key<int>, urd::insertion_ordered>::representation, urd::repr::IndexedSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::positional>::representation, urd::repr::IndexedSet<int>>);
static_assert(urd::has<Store<key<int>, urd::positional>, urd::insertion_ordered, urd::unique>);
static_assert(std::same_as<Store<key<int>, urd::positional, urd::sorted>::representation, FlatSet<int>>);
static_assert(!urd::has<Store<key<int>, urd::insertion_ordered>, urd::positional>);

// A frozen multimap by dense key: compressed sparse rows, each key's values a span.
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen>::representation, urd::repr::CsrMultiMap<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi, urd::dense, urd::frozen, urd::sorted>::representation, urd::repr::CsrMultiMap<int, int>>);
// A dense multimap that grows: a vector of values per index (gap 6 of docs/SURVEY.md: runir's graph builders).
static_assert(std::same_as<Store<key<int>, value<int>, urd::multi, urd::dense>::representation, urd::repr::DenseMultiMap<int, int>>);

// Scopes and layers: a scoped map is a flat hash table with an undo log; a layered Store a flat hash table over a
// parent.
static_assert(std::same_as<Store<key<int>, value<int>, urd::scoped>::representation, urd::repr::ScopedMap<int, int>>);
// Scopes over the map the other properties pick: a tree with sorted, a slot per index with dense.
static_assert(std::same_as<Store<key<int>, value<int>, urd::sorted, urd::scoped>::representation, urd::repr::ScopedTreeMap<int, int>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::dense, urd::scoped>::representation, urd::repr::ScopedDenseMap<int, int>>);
static_assert(std::same_as<Store<key<int>, urd::layered>::representation, urd::repr::Layered<FlatHashSet<int>>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::layered>::representation, urd::repr::Layered<FlatHashMapOf<int, int>>>);
// Layers over the set or map the other properties pick: nodes with stable_refs, a generation map with constant_clear.
static_assert(std::same_as<Store<key<int>, urd::stable_refs, urd::layered>::representation, urd::repr::Layered<HashSet<int>>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::stable_refs, urd::layered>::representation, urd::repr::Layered<HashMapOf<int, int>>>);
static_assert(std::same_as<Store<key<int>, value<int>, urd::dense, urd::constant_clear, urd::layered>::representation,
                           urd::repr::Layered<urd::repr::IndexMap<int, int, true>>>);

static_assert(std::same_as<Store<key<int>, urd::change_log>::representation, urd::repr::ChangeLogged<FlatHashSet<int>>>);
// The log over the set the other properties pick: a tracked bitset for dense keys, a tree for sorted, nodes for
// stable references (gap 4 of docs/SURVEY.md: runir's denotation states, tyr's delta fact sets).
static_assert(std::same_as<Store<key<int>, urd::dense, urd::change_log>::representation, urd::repr::ChangeLogged<urd::repr::DenseSet<int, true>>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::change_log>::representation, urd::repr::ChangeLogged<std::set<int, urd::Less<int>>>>);
static_assert(std::same_as<Store<key<int>, urd::stable_refs, urd::change_log>::representation,
                           urd::repr::ChangeLogged<std::unordered_set<int, urd::Hash<int>, urd::EqualTo<int>>>>);

// Selection trusts the traits, so every registered container whose traits are declared for an element type must offer
// what they promise: checked here for the whole registry, for elements, maps and rows.
template<typename R>
constexpr bool conforms_where_declared()
{
    // Only a declared container is instantiated: an undeclared one may not even be valid for the element type.
    if constexpr (urd::container_traits<R>::declared)
        return urd::conforming<R>;
    else
        return true;
}
template<typename... Rs>
constexpr bool all_conform(urd::detail::type_list<Rs...> /*candidates*/)
{ return (conforms_where_declared<Rs>() && ...); }
template<typename... Ps>
using element_candidates = typename urd::detail::instantiated<typename urd::detail::to_spec<Ps...>::type, urd::detail::element_preference>::type;
template<typename... Ps>
using map_candidates = typename urd::detail::instantiated<typename urd::detail::to_spec<Ps...>::type, urd::detail::map_preference>::type;
// Called parenthesized: argument-dependent lookup would instantiate every candidate to find associated names.
static_assert((all_conform) (element_candidates<key<int>> {}));
static_assert((all_conform) (element_candidates<value<std::string>> {}));
static_assert((all_conform) (element_candidates<value<Store<value<int>, urd::frozen>>> {}));
static_assert((all_conform) (element_candidates<value<Store<key<int>, urd::dense, urd::frozen>>> {}));
static_assert((all_conform) (map_candidates<key<int>, value<int>> {}));
static_assert((all_conform) (map_candidates<key<std::string>, value<double>> {}));

// Sets and maps shared by threads, by hashed keys: gtl's sharded tables (a build without gtl has none).
#if defined(URD_HAS_GTL)
static_assert(std::same_as<Store<key<int>, urd::concurrent>::representation, urd::repr::ParallelHashSet<int>>);
static_assert(std::same_as<Store<key<std::string>, value<int>, urd::concurrent>::representation, urd::repr::ParallelHashMap<std::string, int>>);
// By an indexable key too: without dense, a hash table, not a slot per index.
static_assert(std::same_as<Store<key<int>, value<int>, urd::concurrent>::representation, urd::repr::ParallelHashMap<int, int>>);
// Stability with concurrency on hashed keys: the node tables, whose elements a rehash leaves in place.
static_assert(std::same_as<Store<key<int>, urd::stable_refs, urd::concurrent>::representation, urd::repr::ParallelNodeHashSet<int>>);
static_assert(
    std::same_as<Store<key<std::string>, value<int>, urd::stable_refs, urd::concurrent>::representation, urd::repr::ParallelNodeHashMap<std::string, int>>);
#else
// Without gtl there is no concurrent map by hashed keys, and a dense slot per index is no stand-in: a container that
// stores by index demands keys that are indices, which this request never promised.
static_assert(!urd::detail::choose<urd::detail::to_spec<key<int>, value<int>, urd::concurrent>::type>::found);
#endif
// gtl's table keeps no storage for reuse, and a dense map may not stand in for it (its keys were not promised).
static_assert(!urd::detail::choose<urd::detail::to_spec<key<int>, value<int>, urd::concurrent, urd::retains>::type>::found);

// An interner shared by threads: insertion order, elements that never move, inserts from threads at once.
static_assert(std::same_as<Store<key<int>, urd::positional, urd::monotone, urd::concurrent>::representation, urd::repr::ConcurrentIndexedSet<int>>);
// Elements that never move, without the lock: stability alone buys no synchronization.
static_assert(std::same_as<Store<key<int>, urd::positional, urd::monotone, urd::stable_refs>::representation, urd::repr::StableIndexedSet<int>>);

// Maps in insertion order: hashed keys, or dense ones placed by index with O(1) clear; a dense set in insertion order.
static_assert(std::same_as<Store<key<int>, value<double>, urd::insertion_ordered>::representation, urd::repr::OrderedMap<urd::repr::IndexedSet<int>, double>>);
static_assert(std::same_as<Store<key<int>, value<double>, urd::dense, urd::insertion_ordered>::representation,
                           urd::repr::OrderedMap<urd::repr::SparseSet<int, true>, double>>);
static_assert(urd::has<Store<key<int>, value<double>, urd::dense, urd::insertion_ordered>::representation, urd::constant_clear>);
static_assert(std::same_as<Store<key<int>, urd::dense, urd::insertion_ordered>::representation, urd::repr::SparseSet<int, true>>);

// Rows: a Store of Stores.
static_assert(std::same_as<Store<value<Store<value<int>>>>::representation, std::vector<Store<value<int>>>>);
static_assert(std::same_as<Store<value<Store<value<int>, urd::frozen>>>::representation, urd::repr::Jagged<Store<value<int>, urd::frozen>>>);
static_assert(std::same_as<Store<value<Store<value<Store<value<int>, urd::frozen>>, urd::frozen>>>::representation,
                           urd::repr::Jagged<Store<value<Store<value<int>, urd::frozen>>, urd::frozen>>>);
static_assert(std::same_as<Store<value<Store<value<Store<value<int>, urd::frozen>>, urd::frozen>>>::representation::value_type::representation,
                           urd::repr::Jagged<Store<value<int>, urd::frozen>>>);
static_assert(std::same_as<Store<value<Store<value<int>, urd::retains>>, urd::retains>::representation, urd::repr::RowPool<Store<value<int>, urd::retains>>>);

// Rows of bits whose number and lengths are fixed: one array of words, each row a bitset over its part.
using BitRowSets = Store<value<Store<key<std::size_t>, urd::dense>>, urd::fixed_shape>;
static_assert(std::same_as<BitRowSets::representation, urd::repr::Jagged<Store<key<std::size_t>, urd::dense>>>);
static_assert(std::same_as<BitRowSets::reference, urd::repr::BitsetSpan<std::uint64_t>>);
template<typename S>
concept can_change_rows = requires(S rows, typename S::value_type row) { rows.push_back(row); } || requires(S rows) { rows.clear(); };
static_assert(can_change_rows<Store<value<Store<key<std::size_t>, urd::dense>>>> && !can_change_rows<BitRowSets>);

// Storage kept for reuse, or returned: the containers that do so.
static_assert(std::same_as<Store<key<int>, urd::retains>::representation, FlatHashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::retains>::representation, FlatSet<int>>);
static_assert(std::same_as<Store<value<int>, urd::retains>::representation, std::vector<int>>);
static_assert(std::same_as<Store<key<int>, urd::reclaims>::representation, TreeSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::reclaims>::representation, TreeSet<int>>);

// Stores as hashed keys: those whose contents fix their order (sorted, canonical, sequences) hash by them; an
// unordered one does not, since its iteration order is not its contents' (compile-fail cases 32 and 33).
static_assert(urd::hashable<Store<key<int>, urd::sorted, urd::frozen>> && urd::hashable<Store<key<int>, urd::dense, urd::sorted>>
              && urd::hashable<Store<value<int>>> && urd::hashable<Store<value<int>, urd::frozen>>);
static_assert(!urd::hashable<Store<key<int>>> && !urd::hashable<Store<key<int>, urd::dense, urd::constant_clear>>);
// Compared and ordered by their request alone: any iterable Store compares equal by contents; one iterated in an
// order its request fixes (sorted, insertion_ordered, a sequence) is ordered, so it can be a sorted key.
static_assert(urd::EqualityComparableByEqualTo<Store<key<int>>> && urd::EqualityComparableByEqualTo<Store<value<int>>>);
static_assert(urd::orderable<Store<key<int>, urd::sorted>> && urd::orderable<Store<value<int>>>
              && urd::orderable<Store<key<int>, urd::positional, urd::monotone>> && !urd::orderable<Store<key<int>>>
              && !urd::orderable<Store<value<int>, urd::fifo>>);
using SortedKey = Store<key<int>, urd::sorted, urd::frozen>;
static_assert(std::same_as<Store<key<SortedKey>, value<int>>::representation, FlatHashMapOf<SortedKey, int>>);
using BitKey = Store<key<int>, urd::dense, urd::frozen, urd::sorted>;
static_assert(std::same_as<Store<key<BitKey>>::representation, FlatHashSet<BitKey>>);

// Only what was asked for: a set that reclaims is a tree, which is sorted, but it was not asked to be sorted, so it
// is not.
static_assert(!urd::Sorted<Store<key<int>, urd::reclaims>>);
static_assert(urd::Sorted<Store<key<int>, urd::sorted, urd::small>>);
static_assert(urd::Unique<Store<key<int>, urd::small>>);

// A Store adds no space to its representation: the facets it does not have are empty bases.
static_assert(sizeof(Store<value<int>>) == sizeof(Store<value<int>>::representation));
static_assert(sizeof(Store<key<int>, value<int>, urd::sorted>) == sizeof(Store<key<int>, value<int>, urd::sorted>::representation));

// The interface follows from the properties, in the standard's names.
template<typename S>
concept can_insert = requires(S store, int element) { store.insert(element); };
template<typename S>
concept can_push_back = requires(S store, int element) { store.push_back(element); };
template<typename S>
concept can_erase = requires(S store, int element) { store.erase(store.begin()); };
template<typename S>
concept can_search = requires(const S store, int element) { store.lower_bound(element); };
template<typename S>
concept can_assign = requires(S store, int element) { store[0] = element; };
static_assert(can_insert<Store<key<int>>> && can_erase<Store<key<int>>> && !can_push_back<Store<key<int>>>);
static_assert(!can_insert<Store<key<int>, urd::frozen>> && !can_erase<Store<key<int>, urd::frozen>>);
static_assert(can_push_back<Store<value<int>, urd::monotone>> && !can_erase<Store<value<int>, urd::monotone>>);
static_assert(!can_erase<Store<key<int>, urd::nonempty>>);
// A sorted Store is searched by order; a sorted sequence keeps elements by value, as std::flat_multiset.
static_assert(can_search<Store<key<int>, urd::sorted>> && !can_search<Store<key<int>>>);
static_assert(can_insert<Store<value<int>, urd::sorted>> && !can_push_back<Store<value<int>, urd::sorted>>);
// Elements change in place only where no property depends on their values and the Store may change.
static_assert(can_assign<Store<value<int>>>);
static_assert(!can_assign<Store<value<int>, urd::sorted>> && !can_assign<Store<value<int>, urd::monotone>>);
static_assert(!can_assign<Store<key<int>, urd::positional>>);
// A unique set's insert says whether it inserted; a multiset's does not.
static_assert(std::same_as<decltype(Store<key<int>>().insert(1).second), bool>);
static_assert(std::same_as<decltype(Store<value<int>, urd::sorted>().insert(1)), Store<value<int>, urd::sorted>::iterator>);
// Every Store offers what its properties require of a container.
static_assert(urd::conforming<Store<key<int>>> && urd::conforming<Store<key<int>, urd::sorted>>);
static_assert(urd::conforming<Store<value<int>>> && urd::conforming<Store<value<int>, urd::sorted, urd::nonempty>>);
static_assert(urd::conforming<Store<value<int>, urd::stable_refs, urd::monotone>> && urd::conforming<Store<key<int>, urd::frozen>>);

constexpr int RANGE = 25;
constexpr int STEPS = 2000;

/// Random inserts, erases and lookups on a set Store, against std::set.
template<typename S>
void behaves_like_std_set(unsigned seed)
{
    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, RANGE);
    auto store = S();
    auto reference = std::set<int>();
    for (int step = 0; step < STEPS; ++step)
    {
        const int element = pick(random);
        if (pick(random) % 3 == 0)
        {
            EXPECT_TRUE(store.erase(element) == reference.erase(element));
        }
        else
        {
            EXPECT_TRUE(store.insert(element).second == reference.insert(element).second);
        }
        const int probe = pick(random);
        EXPECT_TRUE(store.contains(probe) == reference.contains(probe));
        EXPECT_TRUE(store.size() == reference.size());
        auto contents = std::vector<int>(store.begin(), store.end());
        std::ranges::sort(contents);
        EXPECT_TRUE(contents == std::vector<int>(reference.begin(), reference.end()));
        if constexpr (urd::Sorted<S>)
        {
            EXPECT_TRUE(std::ranges::is_sorted(store));
        }
    }
}

/// Random inserts and erases on a sorted sequence Store, against std::multiset.
template<typename S>
void behaves_like_std_multiset(unsigned seed)
{
    auto random = std::mt19937(seed);
    auto pick = std::uniform_int_distribution<int>(0, RANGE);
    auto store = S();
    auto reference = std::multiset<int>();
    for (int step = 0; step < STEPS; ++step)
    {
        const int element = pick(random);
        if (pick(random) % 3 == 0)
        {
            EXPECT_TRUE(store.erase(element) == reference.erase(element));
        }
        else
        {
            EXPECT_TRUE(*store.insert(element) == *reference.insert(element));
        }
        EXPECT_TRUE(std::vector<int>(store.begin(), store.end()) == std::vector<int>(reference.begin(), reference.end()));
        const int probe = pick(random);
        EXPECT_TRUE(store.count(probe) == reference.count(probe));
        EXPECT_TRUE(std::distance(store.begin(), store.lower_bound(probe)) == std::distance(reference.begin(), reference.lower_bound(probe)));
    }
}

}  // namespace

namespace
{

/// A key type that names its identity, and nothing else: no operators, nothing in namespace std.
struct Fact
{
    int predicate = 0;
    std::vector<int> objects;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Fact::predicate, &Fact::objects> {}; }
};

}  // namespace

TEST(Store, KeysThatNameTheirIdentity)
{
    const auto facts = std::vector<Fact> { { 3, {} }, { 1, { 2 } }, { 1, { 2 } }, { 1, { 1 } } };
    const auto hashed = Store<key<Fact>>(facts.begin(), facts.end());
    EXPECT_EQ(hashed.size(), 3U);
    EXPECT_TRUE(hashed.contains(Fact { 1, { 2 } }));

    // Ordered member by member: predicate, then objects.
    const auto tree = Store<key<Fact>, urd::sorted>(facts.begin(), facts.end());
    const auto flat = Store<key<Fact>, urd::sorted, urd::frozen>(facts.begin(), facts.end());
    EXPECT_EQ(tree.begin()->objects, std::vector<int>({ 1 }));
    EXPECT_EQ(flat.begin()->objects, std::vector<int>({ 1 }));
    EXPECT_EQ(std::prev(flat.end())->predicate, 3);

    auto counts = Store<key<Fact>, value<int>>();
    counts[Fact { 1, {} }] += 2;
    EXPECT_EQ(counts.at(Fact { 1, {} }), 2);
}

TEST(Store, BehavesLikeTheStandardContainers)
{
    for (unsigned seed = 0; seed < 5; ++seed)
    {
        behaves_like_std_set<Store<key<int>>>(seed);
        behaves_like_std_set<Store<key<int>, urd::sorted>>(seed);
        behaves_like_std_set<Store<key<int>, urd::small>>(seed);
        behaves_like_std_set<Store<key<int>, urd::sorted, urd::small>>(seed);
        behaves_like_std_multiset<Store<value<int>, urd::sorted>>(seed);
    }
}

TEST(Store, BuildsAndChangesAsAsked)
{
    // Built from elements, which the representation puts in the order asked for.
    const auto ids = Store<key<int>, urd::sorted> { 3, 1, 3, 2 };
    EXPECT_TRUE(std::vector<int>(ids.begin(), ids.end()) == std::vector<int>({ 1, 2, 3 }));
    EXPECT_TRUE(*ids.lower_bound(2) == 2 && ids.upper_bound(3) == ids.end() && *ids.rbegin() == 3);
    const auto first = Store<value<int>, urd::sorted, urd::nonempty> { 5, 4 };
    EXPECT_TRUE(first.front() == 4 && first[1] == 5 && first.back() == 5);

    const auto frozen = Store<key<int>, urd::sorted, urd::frozen> { 2, 3, 5 };
    EXPECT_TRUE(frozen.contains(5) && !frozen.contains(4));

    // A plain sequence reads and changes like std::vector.
    auto numbers = Store<value<int>> { 1, 2, 3, 2 };
    numbers[0] = 7;
    numbers.insert(numbers.begin() + 1, 9);
    EXPECT_TRUE(erase(numbers, 2) == 2);
    EXPECT_TRUE(numbers == (Store<value<int>> { 7, 9, 3 }) && numbers.at(2) == 3 && numbers < Store<value<int>> { 8 });
    EXPECT_TRUE(erase_if(numbers, [](int element) { return element > 8; }) == 1 && numbers.size() == 2);
}

namespace
{

template<typename S>
class SetAlgebra : public testing::Test
{
};
using Sets = testing::Types<Store<key<int>>,
                            Store<key<int>, urd::sorted>,
                            Store<key<int>, urd::small>,
                            Store<key<int>, urd::stable_refs>,
                            Store<key<int>, urd::dense>,
                            Store<key<int>, urd::dense, urd::constant_clear>,
                            Store<key<int>, urd::insertion_ordered>,
                            Store<key<int>, urd::dense, urd::proportional_clear>>;
TYPED_TEST_SUITE(SetAlgebra, Sets);

std::vector<int> sorted_copy(const auto& set)
{
    auto result = std::vector<int>(set.begin(), set.end());
    std::ranges::sort(result);
    return result;
}

}  // namespace

// Union, intersection, difference, inclusion and overlap give what std::set_* give on sorted copies.
TYPED_TEST(SetAlgebra, AgreesWithTheStandardAlgorithms)
{
    auto random = std::mt19937(5);
    auto pick = std::uniform_int_distribution<int>(0, 40);
    for (int round = 0; round < 200; ++round)
    {
        auto lhs = TypeParam();
        auto rhs = TypeParam();
        for (int count = pick(random) % 12; count > 0; --count)
            lhs.insert(pick(random));
        for (int count = pick(random) % 12; count > 0; --count)
            rhs.insert(pick(random));
        const auto a = sorted_copy(lhs);
        const auto b = sorted_copy(rhs);

        auto expected = std::vector<int>();
        std::ranges::set_union(a, b, std::back_inserter(expected));
        EXPECT_EQ(sorted_copy(TypeParam(lhs) |= rhs), expected);
        expected.clear();
        std::ranges::set_intersection(a, b, std::back_inserter(expected));
        EXPECT_EQ(sorted_copy(TypeParam(lhs) &= rhs), expected);
        EXPECT_EQ(lhs.intersects(rhs), !expected.empty());
        expected.clear();
        std::ranges::set_difference(a, b, std::back_inserter(expected));
        EXPECT_EQ(sorted_copy(TypeParam(lhs) -= rhs), expected);
        EXPECT_EQ(lhs.includes(rhs), std::ranges::includes(a, b));
        EXPECT_EQ((TypeParam(lhs) |= rhs).size(), sorted_copy(TypeParam(lhs) |= rhs).size());
        // After the algebra, clear leaves nothing behind (a bitset that tracks its words must track theirs too).
        for (auto result : { TypeParam(lhs) |= rhs, TypeParam(lhs) &= rhs, TypeParam(lhs) -= rhs })
        {
            result.clear();
            EXPECT_TRUE(result.empty() && sorted_copy(result).empty());
            result.insert(3);
            EXPECT_EQ(sorted_copy(result), std::vector<int> { 3 });
        }
    }
}

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

// Scopes undo, latest first, every change made in them: random changes and scopes against a stack of copies.
TEST(Scoped, PopUndoesTheScopesChanges)
{
    using Symbols = Store<key<int>, value<int>, urd::scoped>;
    auto random = std::mt19937(3);
    for (int round = 0; round < 50; ++round)
    {
        auto symbols = Symbols();
        auto saved = std::vector<std::map<int, int>>();
        const auto contents = [&symbols] { return std::map<int, int>(symbols.begin(), symbols.end()); };
        for (int step = 0; step < 300; ++step)
        {
            const int key = static_cast<int>(random() % 16);
            const int value = static_cast<int>(random() % 100);
            switch (random() % 7)
            {
                case 0:
                    symbols.push_scope();
                    saved.push_back(contents());
                    break;
                case 1:
                    if (!saved.empty())
                    {
                        symbols.pop_scope();
                        ASSERT_EQ(contents(), saved.back()) << "step " << step;
                        saved.pop_back();
                    }
                    break;
                case 2:
                    symbols.erase(key);
                    break;
                case 3:
                    symbols.insert_or_assign(key, value);
                    break;
                case 4:
                    symbols[key] = value;
                    break;
                case 5:
                    symbols.try_emplace(key, value);
                    break;
                default:
                    if (symbols.contains(key))
                        symbols.at(key) += 1;
                    break;
            }
            ASSERT_EQ(symbols.depth(), saved.size());
        }
    }
}

// A binding is in the innermost scope where that scope made it (loki's redeclaration check, gap 12 of docs/SURVEY.md).
TEST(Scoped, BoundInScopeIsTheInnermostScopesOwn)
{
    auto symbols = Store<key<std::string>, value<int>, urd::scoped>();
    symbols.insert_or_assign("x", 1);
    EXPECT_TRUE(symbols.bound_in_scope("x"));  // no scope open: bound at all
    symbols.push_scope();
    EXPECT_FALSE(symbols.bound_in_scope("x"));  // the outer binding is not this scope's
    symbols.insert_or_assign("x", 2);
    symbols.insert_or_assign("y", 3);
    EXPECT_TRUE(symbols.bound_in_scope("x") && symbols.bound_in_scope("y"));
    symbols.push_scope();
    EXPECT_FALSE(symbols.bound_in_scope("y"));
    symbols.erase("y");
    EXPECT_FALSE(symbols.bound_in_scope("y"));  // changed here, but not bound
    symbols.pop_scope();
    EXPECT_TRUE(symbols.bound_in_scope("y"));
    symbols.pop_scope();
    EXPECT_EQ(symbols.at("x"), 1);
    EXPECT_TRUE(symbols.bound_in_scope("x") && !symbols.contains("y"));
}

// Equal bindings compare equal, whatever order the tables iterate in: one table grew large and shrank back, the
// other never grew, so their slots differ.
TEST(Scoped, EqualBindingsCompareEqual)
{
    using Symbols = Store<key<int>, value<int>, urd::scoped>;
    auto grown = Symbols();
    for (int key = 0; key < 4096; ++key)
        grown.insert_or_assign(key, key);
    for (int key = 4096 - 1; key >= 0; --key)
        if (key % 512 != 0)
            grown.erase(key);
    auto small = Symbols();
    for (int key = 4096 - 512; key >= 0; key -= 512)
        small.insert_or_assign(key, key);
    small.push_scope();  // open scopes do not count: only the bindings in force
    EXPECT_FALSE(std::ranges::equal(grown, small));
    EXPECT_TRUE(grown == small);
    small.insert_or_assign(0, 1);
    EXPECT_FALSE(grown == small);
}

// A layered Store has no ==: its contents include its parent's (store/denials.hpp names why).
static_assert(!std::equality_comparable<Store<key<int>, urd::layered>> && std::equality_comparable<Store<key<int>>>);

// A priority queue with handles gives the handle from emplace as from push, to update the element later.
TEST(Queues, EmplaceGivesTheHandle)
{
    auto queue = Store<value<int>, urd::by_priority, urd::handles>();
    const auto handle = queue.emplace(5);
    queue.push(3);
    queue.update(handle, 1);
    EXPECT_EQ(queue.top(), 1);
}

// An insertion-ordered set keeps its elements in one array, so it has positions without asking for positional.
TEST(Positions, InsertionOrderedSetsHaveThem)
{
    auto names = Store<key<std::string>, urd::insertion_ordered>();
    names.insert("b");
    names.insert("a");
    EXPECT_EQ(names[0], "b");
    EXPECT_EQ(names.back(), "a");
    EXPECT_EQ(names.position(names.find("a")), 1U);
}

// A sorted Store's least and greatest elements are its first and last, O(1) in a tree or a sorted array.
TEST(Ends, SortedStoresHaveTheirLeastAndGreatest)
{
    const auto tree = Store<key<int>, urd::sorted> { 5, 1, 3 };
    EXPECT_EQ(tree.front(), 1);
    EXPECT_EQ(tree.back(), 5);
    const auto flat = Store<key<int>, urd::sorted, urd::frozen> { 5, 1, 3 };
    EXPECT_EQ(flat.front(), 1);
    const auto map = Store<key<int>, value<int>, urd::sorted> { { 2, 20 }, { 1, 10 } };
    EXPECT_EQ(map.front().second, 10);
    EXPECT_EQ(map.back().first, 2);
}
template<typename S>
concept has_front = requires(const S& store) { store.front(); };
static_assert(!has_front<Store<key<int>, urd::dense, urd::sorted>> && !has_front<Store<key<int>>>);

// The change log answers whether an element is among its changes, without scanning them.
TEST(ChangeLog, WasAddedAndWasRemoved)
{
    auto facts = Store<key<int>, urd::change_log> { 1, 2 };
    facts.clear_changes();
    facts.insert(3);
    facts.erase(1);
    EXPECT_TRUE(facts.was_added(3) && !facts.was_added(2) && facts.was_removed(1) && !facts.was_removed(3));
}

// A queue visits its elements: in the order pushed (fifo, lifo), or in no particular order (by priority).
TEST(Queues, IterateTheirElements)
{
    const auto pushed = std::vector { 4, 1, 3, 1 };
    auto fifo = Store<value<int>, urd::fifo>();
    auto ring = Store<value<int>, urd::fifo, urd::retains>();
    auto lifo = Store<value<int>, urd::lifo>();
    auto heap = Store<value<int>, urd::by_priority>();
    auto handled = Store<value<int>, urd::by_priority, urd::handles>();
    for (const auto each : pushed)
    {
        fifo.push(each);
        ring.push(each);
        lifo.push(each);
        heap.push(each);
        handled.push(each);
    }
    fifo.pop();
    ring.pop();
    EXPECT_TRUE(std::ranges::equal(fifo, std::vector { 1, 3, 1 }));
    EXPECT_TRUE(std::ranges::equal(ring, std::vector { 1, 3, 1 }));
    EXPECT_EQ(fifo.back(), 1);
    EXPECT_EQ(ring.back(), 1);
    EXPECT_TRUE(std::ranges::equal(lifo, pushed));
    const auto sorted = [](const auto& queue)
    {
        auto elements = std::vector<int>(queue.begin(), queue.end());
        std::ranges::sort(elements);
        return elements;
    };
    EXPECT_EQ(sorted(heap), (std::vector { 1, 1, 3, 4 }));
    EXPECT_EQ(sorted(handled), (std::vector { 1, 1, 3, 4 }));
}
template<typename S>
concept iterates = requires(const S& store) { store.begin(); };
static_assert(iterates<Store<value<int>, urd::fifo>> && !iterates<Store<value<int>, urd::by_priority, urd::monotone_priority>>);

// A priority queue with handles reads an element by its handle, wherever it is in the order.
TEST(Queues, ReadByHandle)
{
    auto queue = Store<value<int>, urd::by_priority, urd::handles>();
    const auto five = queue.push(5);
    queue.push(1);
    EXPECT_EQ(queue[five], 5);
    queue.update(five, 0);
    EXPECT_EQ(queue[five], 0);
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

// A layer finds its parent's elements, holds only its own, and shadows the parent's values with its own.
TEST(Layered, AnswersFromItselfThenItsParent)
{
    using Names = Store<key<std::string>, value<int>, urd::layered>;
    const auto parent = Names { { "on", 0 }, { "clear", 1 } };
    auto child = Names { { "holding", 2 } };
    child.layer_over(&parent);
    EXPECT_TRUE(child.contains("on") && child.contains("holding") && !child.contains("empty"));
    EXPECT_EQ(child.size(), 1U);
    EXPECT_EQ(std::as_const(child).at("clear"), 1);
    EXPECT_FALSE(child.try_emplace("on", 7).second);
    EXPECT_EQ(std::as_const(child).at("on"), 0);
    child.insert_or_assign("on", 5);
    EXPECT_EQ(child.at("on"), 5);
    EXPECT_EQ(parent.at("on"), 0);
    EXPECT_EQ(child.erase("clear"), 0U);
    EXPECT_TRUE(child.contains("clear"));
    // at() on a layer that may change hands out its own value to change: the parent's is copied up first.
    child.at("clear") = 3;
    EXPECT_EQ(child.size(), 3U);
    EXPECT_EQ(parent.at("clear"), 1);

    using Seen = Store<key<int>, urd::layered>;
    const auto before = Seen { 1, 2 };
    auto after = Seen { 3 };
    after.layer_over(&before);
    EXPECT_FALSE(after.insert(2).second);
    EXPECT_TRUE(after.insert(4).second);
    EXPECT_EQ(std::set<int>(after.begin(), after.end()), (std::set<int> { 3, 4 }));
    after.layer_over(nullptr);
    EXPECT_FALSE(after.contains(1));
}

// A two-level dense map is two dense Stores nested: cleared at the outer level in O(1), refilled fresh.
TEST(Nested, TwoLevelDenseMap)
{
    using Rows = Store<key<int>, value<double>, urd::dense>;
    auto relations = Store<key<int>, value<Rows>, urd::dense, urd::constant_clear>();
    relations[2][5] = 1.5;
    relations[2][7] = 2.5;
    relations[0][1] = 0.5;
    EXPECT_EQ(relations.at(2).size(), 2U);
    relations.clear();
    EXPECT_TRUE(relations.empty());
    relations[2][1] = 3.0;
    EXPECT_EQ(relations.at(2).size(), 1U);
    EXPECT_FALSE(relations.at(2).contains(5));
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

namespace
{

template<typename T>
using Interned = Store<key<T>, urd::positional, urd::monotone>;

}  // namespace

// A family holds one Store per type and reaches each by its type.
TEST(Family, ReachesEachMemberByType)
{
    auto repositories = urd::compose::Family<Interned, int, std::string>();
    repositories.get<int>().insert(7);
    repositories.get<std::string>().insert("on");
    repositories.get<std::string>().insert("clear");
    EXPECT_EQ(repositories.get<int>().size(), 1U);
    EXPECT_EQ(std::as_const(repositories).get<std::string>()[1], "clear");
    std::size_t total = 0;
    repositories.for_each([&total](const auto& member) { total += member.size(); });
    EXPECT_EQ(total, 3U);
}

// A change log holds exactly the difference between the contents and those when it was last cleared.
TEST(ChangeLog, HoldsTheDifferenceSinceItWasCleared)
{
    auto random = std::mt19937(29);
    auto facts = Store<key<int>, urd::change_log>();
    auto reference = std::set<int>();
    for (int step = 0; step < 2000; ++step)
    {
        const int element = static_cast<int>(random() % 20);
        switch (random() % 10)
        {
            case 0:
                facts.clear_changes();
                reference = std::set<int>(facts.begin(), facts.end());
                break;
            case 1:
                if (random() % 8 == 0)
                    facts.clear();
                break;
            case 2:
            case 3:
            case 4:
                facts.erase(element);
                break;
            default:
                facts.insert(element);
                break;
        }
        const auto now = std::set<int>(facts.begin(), facts.end());
        auto added = std::vector<int>();
        auto removed = std::vector<int>();
        std::ranges::set_difference(now, reference, std::back_inserter(added));
        std::ranges::set_difference(reference, now, std::back_inserter(removed));
        ASSERT_EQ(std::set<int>(facts.added().begin(), facts.added().end()), std::set<int>(added.begin(), added.end())) << "step " << step;
        ASSERT_EQ(facts.added().size(), added.size());
        ASSERT_EQ(std::set<int>(facts.removed().begin(), facts.removed().end()), std::set<int>(removed.begin(), removed.end())) << "step " << step;
        ASSERT_EQ(facts.removed().size(), removed.size());
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

// An insertion-ordered map gives its keys and values as spans in insertion order, and an entry's position.
TEST(Entries, KeysAndValuesInInsertionOrder)
{
    auto costs = Store<key<int>, value<double>, urd::dense, urd::insertion_ordered>();
    costs.try_emplace(7, 1.5);
    costs.try_emplace(2, 0.5);
    costs.insert_or_assign(7, 2.5);
    costs[9] = 4.0;
    EXPECT_TRUE(std::ranges::equal(costs.keys(), std::vector<int> { 7, 2, 9 }));
    EXPECT_TRUE(std::ranges::equal(costs.values(), std::vector<double> { 2.5, 0.5, 4.0 }));
    EXPECT_EQ(costs.position(costs.find(2)), 1U);
    EXPECT_EQ(costs.position(costs.find(5)), costs.size());
    costs.erase(7);
    EXPECT_TRUE(std::ranges::equal(costs.keys(), std::vector<int> { 2, 9 }));
    EXPECT_EQ(costs.at(9), 4.0);
}

namespace
{

/// A closed interval, merged by hull.
struct Interval
{
    double lower = 0;
    double upper = 0;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Interval::lower, &Interval::upper> {}; }
};
constexpr auto hull = [](const Interval& lhs, const Interval& rhs) { return Interval { std::min(lhs.lower, rhs.lower), std::max(lhs.upper, rhs.upper) }; };

template<typename S>
class Merge : public testing::Test
{
};
using MergeMaps = testing::Types<Store<key<int>, value<int>>,
                                 Store<key<int>, value<int>, urd::sorted>,
                                 Store<key<int>, value<int>, urd::dense>,
                                 Store<key<int>, value<int>, urd::dense, urd::constant_clear>,
                                 Store<key<int>, value<int>, urd::insertion_ordered>,
                                 Store<key<int>, value<int>, urd::dense, urd::insertion_ordered>,
                                 Store<key<int>, value<int>, urd::scoped>>;
TYPED_TEST_SUITE(Merge, MergeMaps);

}  // namespace

// merge inserts where a key has no value, combines where it has one, and says whether the map changed: against a
// std::map merged by hand.
TYPED_TEST(Merge, CombinesAndReportsChange)
{
    auto random = std::mt19937(41);
    auto map = TypeParam();
    auto expected = std::map<int, int>();
    const auto lower = [](int lhs, int rhs) { return std::min(lhs, rhs); };
    for (int step = 0; step < 2000; ++step)
    {
        const int key = static_cast<int>(random() % 30);
        const int value = static_cast<int>(random() % 100);
        const auto [found, inserted] = expected.try_emplace(key, value);
        const bool changed = inserted || value < found->second;
        found->second = std::min(found->second, value);
        ASSERT_EQ(map.merge(key, value, lower), changed) << "step " << step;
        ASSERT_EQ(map.at(key), expected.at(key));
    }
    EXPECT_EQ(map.size(), expected.size());
}

// An interval widened by hull only where it grows.
TEST(Merge, HullOfIntervals)
{
    auto values = Store<key<int>, value<Interval>, urd::dense, urd::insertion_ordered>();
    EXPECT_TRUE(values.merge(3, Interval { 1, 2 }, hull));
    EXPECT_FALSE(values.merge(3, Interval { 1.5, 2 }, hull));
    EXPECT_TRUE(values.merge(3, Interval { 0, 1 }, hull));
    EXPECT_EQ(values.at(3).lower, 0);
    EXPECT_EQ(values.at(3).upper, 2);
}

// unite says whether the union added anything.
TYPED_TEST(SetAlgebra, UniteReportsWhetherItAdded)
{
    auto random = std::mt19937(43);
    for (int round = 0; round < 200; ++round)
    {
        auto lhs = TypeParam();
        auto rhs = TypeParam();
        for (int count = static_cast<int>(random() % 8); count > 0; --count)
            lhs.insert(static_cast<int>(random() % 20));
        for (int count = static_cast<int>(random() % 8); count > 0; --count)
            rhs.insert(static_cast<int>(random() % 20));
        const bool adds = !lhs.includes(rhs);
        EXPECT_EQ(lhs.unite(rhs), adds);
        EXPECT_TRUE(lhs.includes(rhs));
        EXPECT_FALSE(lhs.unite(rhs));
    }
}

// update hands each value to a function with whether the key had one; read reads it or gives a default. Against a
// std::map updated by hand, over the map kinds of the merge suite.
TYPED_TEST(Merge, UpdateAndRead)
{
    auto random = std::mt19937(47);
    auto map = TypeParam();
    auto expected = std::map<int, int>();
    for (int step = 0; step < 2000; ++step)
    {
        const int key = static_cast<int>(random() % 30);
        const bool had = expected.contains(key);
        const auto reported = map.update(key,
                                         [](int& count, bool held)
                                         {
                                             count = held ? count + 1 : 1;
                                             return held;
                                         });
        ASSERT_EQ(reported, had) << "step " << step;
        expected[key] = had ? expected[key] + 1 : 1;
        const int probe = static_cast<int>(random() % 32);
        ASSERT_EQ(map.read(probe, [](int count) { return count; }, -1), expected.contains(probe) ? expected[probe] : -1);
    }
}

// A map cleared by generation hands over the value it kept, storage and all: a buffer refilled per round allocates
// once.
TEST(Merge, GenerationsReuseValues)
{
    auto buffers = Store<key<int>, value<std::vector<int>>, urd::dense, urd::constant_clear>();
    buffers.update(3, [](std::vector<int>& buffer, bool) { buffer.assign(1000, 7); });
    buffers.clear();
    EXPECT_FALSE(buffers.contains(3));
    buffers.update(3,
                   [](std::vector<int>& buffer, bool held)
                   {
                       EXPECT_FALSE(held);
                       EXPECT_GE(buffer.capacity(), 1000U);
                       buffer.clear();
                   });
    EXPECT_TRUE(buffers.contains(3));
    EXPECT_TRUE(buffers.at(3).empty());
}

// Threads updating and reading the same keys of a thread-safe generation map at once: every update counts.
TEST(Merge, ThreadsUpdateTogether)
{
    auto counts = Store<key<int>, value<int>, urd::dense, urd::constant_clear, urd::concurrent>();
    static_assert(std::same_as<decltype(counts)::representation, urd::repr::IndexMap<int, int, true, true>>);
    constexpr int threads = 4;
    constexpr int rounds = 20000;
    constexpr int keys = 50;
    {
        auto workers = std::vector<std::jthread>();
        for (int thread = 0; thread < threads; ++thread)
            workers.emplace_back(
                [&counts, thread]
                {
                    for (int round = 0; round < rounds; ++round)
                    {
                        counts.update((round + thread) % keys, [](int& count, bool held) { count = held ? count + 1 : 1; });
                        (void) counts.read(round % keys, [](int count) { return count; }, 0);
                    }
                });
    }
    int total = 0;
    for (int key = 0; key < keys; ++key)
        total += counts.at(key);
    EXPECT_EQ(total, threads * rounds);
}

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

// Stores as sorted keys: ordered lexicographically by their elements, equal by contents.
TEST(Less, StoresAsKeys)
{
    using Inner = Store<key<int>, urd::sorted>;
    const auto less = urd::Less<Inner> {};
    EXPECT_TRUE(less(Inner { 1, 2 }, Inner { 1, 3 }));
    EXPECT_TRUE(less(Inner { 1, 2 }, Inner { 1, 2, 3 }));
    EXPECT_FALSE(less(Inner { 2, 1 }, Inner { 1, 2 }));
    EXPECT_TRUE(urd::EqualTo<Inner> {}(Inner { 2, 1 }, Inner { 1, 2 }));

    auto by_contents = Store<key<Inner>, urd::sorted>();
    by_contents.insert(Inner { 3 });
    by_contents.insert(Inner { 1, 2 });
    by_contents.insert(Inner { 2, 1 });
    EXPECT_EQ(by_contents.size(), 2u);
    EXPECT_EQ(*by_contents.begin(), (Inner { 1, 2 }));
}

// Stores as hashed keys: found whatever order the key was built in, and whatever storage a bitset grew to.
TEST(Hash, StoresAsKeys)
{
    auto plans = Store<key<SortedKey>, value<int>>();
    plans.insert({ SortedKey { 3, 1, 2 }, 7 });
    EXPECT_EQ(plans.at(SortedKey { 1, 2, 3 }), 7);
    EXPECT_FALSE(plans.contains(SortedKey { 1, 2 }));

    auto grown = Store<key<int>, urd::dense, urd::sorted> { 1, 2 };
    grown.insert(200);
    grown.erase(200);
    const auto small = Store<key<int>, urd::dense, urd::sorted> { 1, 2 };
    EXPECT_EQ(grown, small);
    using Key = Store<key<int>, urd::dense, urd::sorted>;
    EXPECT_EQ(urd::Hash<Key> {}(grown), urd::Hash<Key> {}(small));
    auto seen = Store<key<Key>>();
    seen.insert(grown);
    EXPECT_TRUE(seen.contains(small));
}

// Threads updating and reading the same hashed keys of a shared map at once: every update counts.
#if defined(URD_HAS_GTL)
TEST(Merge, ThreadsUpdateHashedKeysTogether)
{
    auto counts = Store<key<std::string>, value<int>, urd::concurrent>();
    constexpr int threads = 4;
    constexpr int rounds = 20000;
    constexpr int keys = 50;
    {
        auto workers = std::vector<std::jthread>();
        for (int thread = 0; thread < threads; ++thread)
            workers.emplace_back(
                [&counts, thread]
                {
                    for (int round = 0; round < rounds; ++round)
                    {
                        counts.update(std::to_string((round + thread) % keys), [](int& count, bool held) { count = held ? count + 1 : 1; });
                        (void) counts.read(std::to_string(round % keys), [](int count) { return count; }, 0);
                    }
                });
    }
    int total = 0;
    for (int key = 0; key < keys; ++key)
        total += counts.at(std::to_string(key));
    EXPECT_EQ(total, threads * rounds);
}
#endif

// Ordered by a projection: a node's cost, ties by the node itself; a priority queue by (f, larger g first).
namespace
{
struct Node
{
    int id;
    int f;
    int g;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Node::id> {}; }
};
}  // namespace

TEST(OrderBy, SortsByTheProjectionAndBreaksTiesByTheElement)
{
    using ByF = Store<key<Node>, urd::sorted, urd::order_by<&Node::f>>;
    static_assert(std::same_as<ByF::representation, std::set<Node, urd::ProjectedLess<Node, &Node::f>>>);
    auto nodes = ByF { Node { 1, 5, 0 }, Node { 2, 3, 0 }, Node { 3, 5, 0 }, Node { 4, 1, 0 } };
    auto ids = std::vector<int>();
    for (const auto& node : nodes)
        ids.push_back(node.id);
    EXPECT_EQ(ids, (std::vector<int> { 4, 2, 1, 3 }));
    EXPECT_EQ(nodes.size(), 4U);
    EXPECT_TRUE(nodes.contains(Node { 3, 5, 0 }));
}

TEST(OrderBy, MapsOrderTheirKeysByTheProjection)
{
    auto costs = Store<key<Node>, value<std::string>, urd::sorted, urd::order_by<&Node::f>>();
    costs.insert_or_assign(Node { 1, 9, 0 }, "late");
    costs.insert_or_assign(Node { 2, 2, 0 }, "early");
    EXPECT_EQ(costs.begin()->second, "early");
}

TEST(OrderBy, PriorityQueuePopsByTheProjection)
{
    using Open = Store<value<Node>, urd::by_priority, urd::order_by<[](const Node& node) { return std::tuple(node.f, -node.g); }>>;
    auto open = Open();
    open.push(Node { 1, 4, 1 });
    open.push(Node { 2, 4, 3 });
    open.push(Node { 3, 2, 0 });
    auto ids = std::vector<int>();
    while (!open.empty())
    {
        ids.push_back(open.top().id);
        open.pop();
    }
    EXPECT_EQ(ids, (std::vector<int> { 3, 2, 1 }));
}

// A queue's elements need no order of their own where a projection gives one.
TEST(OrderBy, QueueOfUnorderedElements)
{
    struct Task
    {
        int cost;
        std::string name;
    };
    auto tasks = Store<value<Task>, urd::by_priority, urd::order_by<&Task::cost>>();
    tasks.push(Task { 7, "b" });
    tasks.push(Task { 2, "a" });
    EXPECT_EQ(tasks.top().name, "a");
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

// A sorted map whose values are rows that keep their memory: erased keys leave their nodes and values for the next
// keys, so buckets of a cost queue, emptied and refilled, allocate nothing once warm.
TEST(Recycling, BucketsRefillWithoutAllocating)
{
    using Bucket = Store<key<int>, urd::retains>;
    using Buckets = Store<key<double>, value<Bucket>, urd::sorted>;
    static_assert(std::same_as<Buckets::representation, urd::repr::RecyclingMap<double, Bucket>>);
    // Values that may free their memory stay in a plain tree.
    static_assert(std::same_as<Store<key<double>, value<Store<key<int>>>, urd::sorted>::representation, TreeMapOf<double, Store<key<int>>>>);
    auto buckets = Buckets();
    const auto fill = [&buckets](double cost)
    {
        for (int each = 0; each < 100; ++each)
            buckets[cost].insert(each + static_cast<int>(cost * 1000));
    };
    for (int round = 0; round < 3; ++round)
    {
        fill(1.0 + round);
        fill(2.0 + round);
        buckets.erase(buckets.begin()->first);
        buckets.erase(buckets.begin()->first);
    }
    {
        const auto counted = urd::test::Counted();
        fill(7.5);
        fill(8.5);
    }
    if (urd::test::counts_allocations)
    {
        EXPECT_EQ(urd::test::allocations(), 0);
    }
    EXPECT_EQ(buckets.size(), 2U);
    EXPECT_EQ(buckets.begin()->first, 7.5);
    EXPECT_EQ(buckets.at(8.5).size(), 100U);
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

// Without an order asked for, values that keep their memory get a hash map that recycles: no tree where none is needed.
// Sets with stable references that keep their storage: the node sets, their erased nodes kept for the next elements.
static_assert(std::same_as<Store<key<int>, urd::stable_refs, urd::retains>::representation, urd::repr::RecyclingHashSet<int>>);
static_assert(std::same_as<Store<key<int>, urd::sorted, urd::stable_refs, urd::retains>::representation, urd::repr::RecyclingTreeSet<int>>);
// Without stable_refs, retains is the flat containers' already.
static_assert(std::same_as<Store<key<int>, urd::retains>::representation, boost::unordered_flat_set<int, urd::Hash<int>, urd::EqualTo<int>>>);

TEST(Recycling, UnorderedMapsRecycleToo)
{
    using Bucket = Store<key<int>, urd::retains>;
    using Groups = Store<key<int>, value<Bucket>>;
    static_assert(std::same_as<Groups::representation, urd::repr::RecyclingHashMap<int, Bucket>>);
    auto groups = Groups();
    for (int round = 0; round < 3; ++round)
    {
        for (int each = 0; each < 50; ++each)
            groups[round].insert(each + (round * 100));
        groups.erase(round);
    }
    {
        const auto counted = urd::test::Counted();
        for (int each = 0; each < 50; ++each)
            groups[7].insert(each + 700);
    }
    if (urd::test::counts_allocations)
    {
        EXPECT_EQ(urd::test::allocations(), 0);
    }
    EXPECT_EQ(groups.at(7).size(), 50U);
    EXPECT_FALSE(groups.at(7).contains(0));
}

// Entries identified by one thing and ordered by another: offers per interval, kept cheapest first. merge inserts or
// keeps the cheaper; every change puts the entry back in order; equal costs go by the interval.
namespace
{
struct Span
{
    int lower;
    int upper;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Span::lower, &Span::upper> {}; }
};
struct Offer
{
    double cost;
    int source;
    friend bool operator==(const Offer&, const Offer&) = default;
};
constexpr auto cheapest_first = [](const Span& span, const Offer& offer) { return std::tuple(offer.cost, span.lower, span.upper); };
using Offers = Store<key<Span>, value<Offer>, urd::sorted, urd::small, urd::order_by<cheapest_first>>;
template<typename S>
concept changes_values_by_reference = requires(S offers, Span span) { offers.at(span).cost = 0; } || requires(S offers, Span span) { offers[span]; };
}  // namespace

TEST(OrderByValue, KeepsEntriesInTheOrderTheirValuesGive)
{
    static_assert(std::same_as<Offers::representation, urd::repr::FlatEntryMap<Span, Offer, urd::EntryLess<Span, Offer, cheapest_first>>>);
    static_assert(!changes_values_by_reference<Offers> && changes_values_by_reference<Store<key<Span>, value<Offer>, urd::sorted>>);
    const auto cheaper = [](const Offer& held, const Offer& next) { return next.cost < held.cost ? next : held; };
    auto offers = Offers();
    EXPECT_TRUE(offers.merge(Span { 0, 4 }, Offer { 5.0, 1 }, cheaper));
    EXPECT_TRUE(offers.merge(Span { 2, 3 }, Offer { 2.0, 2 }, cheaper));
    EXPECT_TRUE(offers.merge(Span { 1, 1 }, Offer { 2.0, 3 }, cheaper));
    EXPECT_FALSE(offers.merge(Span { 0, 4 }, Offer { 7.0, 4 }, cheaper));  // not cheaper: unchanged
    const auto sources = [&offers]
    {
        auto order = std::vector<int>();
        for (const auto& [span, offer] : offers)
            order.push_back(offer.source);
        return order;
    };
    EXPECT_EQ(sources(), (std::vector<int> { 3, 2, 1 }));                 // equal costs by span: {1, 1} before {2, 3}
    EXPECT_TRUE(offers.merge(Span { 0, 4 }, Offer { 1.0, 5 }, cheaper));  // cheaper: moves to the front
    EXPECT_EQ(sources(), (std::vector<int> { 5, 3, 2 }));
    offers.update(Span { 1, 1 }, [](Offer& offer, bool) { offer.cost = 9.0; });
    EXPECT_EQ(sources(), (std::vector<int> { 5, 2, 3 }));
    EXPECT_EQ(std::prev(offers.end())->first.lower, 1);  // the most expensive last
    EXPECT_EQ(offers.at(Span { 2, 3 }).source, 2);       // found by its span
    EXPECT_EQ(offers.read(Span { 7, 7 }, [](const Offer& offer) { return offer.source; }, -1), -1);
    offers.insert_or_assign(Span { 2, 3 }, Offer { 0.5, 6 });
    EXPECT_EQ(sources(), (std::vector<int> { 6, 5, 3 }));
}

// Entries whose projections tie come in key order, whatever order they were inserted in.
TEST(OrderByValue, TiesGoByKey)
{
    auto ranked = Store<key<int>, value<int>, urd::sorted, urd::small, urd::order_by<[](int, int score) { return score; }>>();
    for (const int key : { 5, 1, 3, 4, 2 })
        ranked.insert_or_assign(key, key % 2);
    auto keys = std::vector<int>();
    for (const auto& [key, score] : ranked)
        keys.push_back(key);
    EXPECT_EQ(keys, (std::vector<int> { 2, 4, 1, 3, 5 }));
}

// Relations as yggdrasil's relation repositories keep them: per predicate, a pool of the rows (tuples of object
// indices) bound to it, each row once; a phase layers over the previous through both levels: a predicate absent
// from the layer falls through to the parent's pool, a predicate touched in the layer gets a pool of its own over
// the parent's pool, with indices continuing after it.
namespace
{

struct PredicateTag;
struct ObjectTag;
using Object = urd::index<ObjectTag>;
using Row = Store<value<Object>, urd::frozen>;
using Rows = Store<value<Row>, urd::handles, urd::unique, urd::monotone, urd::layered>;
using Relations = Store<key<urd::index<PredicateTag>>, value<Rows>, urd::dense, urd::layered>;
static_assert(std::same_as<Relations::representation, urd::repr::Layered<urd::repr::IndexMap<urd::index<PredicateTag>, Rows, false>>>);

}  // namespace

TEST(Layered, RelationsLayerThroughBothLevels)
{
    const auto on = urd::index<PredicateTag>(0);
    const auto clear = urd::index<PredicateTag>(1);
    const auto holding = urd::index<PredicateTag>(2);
    const auto a = Object(0);
    const auto b = Object(1);
    auto domain = Relations();
    const auto on_ab = domain[on].insert({ a, b }).first;
    const auto on_ba = domain[on].insert({ b, a }).first;
    const auto clear_a = domain[clear].insert({ a }).first;
    EXPECT_EQ(on_ab, urd::index<Row>(0));
    EXPECT_EQ(on_ba, urd::index<Row>(1));
    EXPECT_EQ(clear_a, urd::index<Row>(0));

    auto task = Relations();
    task.layer_over(&domain);
    EXPECT_TRUE(task.contains(on) && task.contains(clear) && !task.contains(holding));
    EXPECT_EQ(task.size(), 0U);
    EXPECT_EQ(std::as_const(task).at(clear).row_size(clear_a), 1U);  // read through: the domain's pool
    EXPECT_EQ(task[on].insert({ a, b }), std::pair(on_ab, false));   // the domain's row, through the task's own pool over it
    EXPECT_EQ(task.size(), 1U);                                      // which now exists, layered
    EXPECT_EQ(task[on].base(), 2U);
    EXPECT_EQ(task[on].layer(), 1U);
    const auto on_aa = task[on].insert({ a, a }).first;
    EXPECT_EQ(on_aa, urd::index<Row>(2));
    const auto expected = Row { a, b };
    EXPECT_EQ(task[on].get(on_ab), expected);
    EXPECT_EQ(task[on].row_size(on_aa), 2U);
    EXPECT_EQ(std::as_const(domain).at(on).row_size(on_ba), 2U);  // the domain's pool, untouched: still two rows at the branch
    EXPECT_FALSE(task[on].parent_grew());
    const auto holding_b = task[holding].insert({ b }).first;  // a predicate new in the task: a root pool of its own
    EXPECT_EQ(holding_b, urd::index<Row>(0));
    EXPECT_EQ(task[holding].layer(), 0U);
    EXPECT_EQ(task.size(), 2U);
}
