// What a Store is, checked at compile time: the order of a request's properties does not matter, what a request
// implies, the interface its properties give (in the standard's names), which Stores hash, compare and order, and
// that a Store adds no space to its representation. What each request resolves to is docs/RESOLUTION.md, checked by
// the resolution test; only what that table cannot show is here.

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
// Few elements: contiguous storage first, node-based containers last, a hash table before a tree among them.
// Elements that must not move: segments, unless nothing changes anyway.
// Maps: by key, as the sets are by element.
// A multimap is postings: a key's values together, one probe away. Stable references need the node-based table.
// A map of rows holds the inner Stores; flattening by dense key comes with dense maps.
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
// A bitset that clears only the words it set: sorted, cleared in time proportional to its elements.
// Values that never move: the slots in segments, without the lock the thread-safe map pays.

// Insertion order and positions: an indexed set. A set that is positional keeps insertion order unless it asks to be
// sorted, so that an element's position is its index (an interner).
static_assert(urd::has<Store<key<int>, urd::positional>, urd::insertion_ordered, urd::unique>);
static_assert(!urd::has<Store<key<int>, urd::insertion_ordered>, urd::positional>);

// A frozen multimap by dense key: compressed sparse rows, each key's values a span.
// A dense multimap that grows: a vector of values per index (runir's graph builders).

// Scopes and layers: a scoped map is a flat hash table with an undo log; a layered Store a flat hash table over a
// parent.
// Scopes over the map the other properties pick: a tree with sorted, a slot per index with dense.
// Layers over the set or map the other properties pick: nodes with stable_refs, a generation map with constant_clear.

// The log over the set the other properties pick: a tracked bitset for dense keys, a tree for sorted, nodes for
// stable references (runir's denotation states, tyr's delta fact sets).

// Sets and maps shared by threads, by hashed keys: gtl's sharded tables.
// By an indexable key too: without dense, a hash table, not a slot per index.
// Stability with concurrency on hashed keys: the node tables, whose elements a rehash leaves in place.
// gtl's table keeps no storage for reuse, and a dense map may not stand in for it (its keys were not promised).
static_assert(!urd::detail::choose<urd::detail::to_spec<key<int>, value<int>, urd::concurrent, urd::retains>::type>::found);

// An interner shared by threads: insertion order, elements that never move, inserts from threads at once.
// Elements that never move, without the lock: stability alone buys no synchronization.

// Maps in insertion order: hashed keys, or dense ones placed by index with O(1) clear; a dense set in insertion order.
static_assert(urd::has<Store<key<int>, value<double>, urd::dense, urd::insertion_ordered>::representation, urd::constant_clear>);

// Rows: a Store of Stores.

// Rows of bits whose number and lengths are fixed: one array of words, each row a bitset over its part.
using BitRowSets = Store<value<Store<key<std::size_t>, urd::dense>>, urd::fixed_shape>;
static_assert(std::same_as<BitRowSets::reference, urd::repr::BitsetSpan<std::uint64_t>>);
template<typename S>
concept can_change_rows = requires(S rows, typename S::value_type row) { rows.push_back(row); } || requires(S rows) { rows.clear(); };
static_assert(can_change_rows<Store<value<Store<key<std::size_t>, urd::dense>>>> && !can_change_rows<BitRowSets>);

// Storage kept for reuse, or returned: the containers that do so.

// Stores as hashed keys: those whose contents fix their order (sorted, canonical, sequences) hash by them; an
// unordered one does not, since its iteration order is not its contents' (compile-fail cases 28 and 29).
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
using BitKey = Store<key<int>, urd::dense, urd::frozen, urd::sorted>;

// Only what was asked for: a set that reclaims is a tree, which is sorted, but it was not asked to be sorted, so it
// is not.
static_assert(!urd::has<Store<key<int>, urd::reclaims>, urd::sorted>);
static_assert(urd::has<Store<key<int>, urd::sorted, urd::small>, urd::sorted>);
static_assert(urd::has<Store<key<int>, urd::small>, urd::unique>);

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

}  // namespace
