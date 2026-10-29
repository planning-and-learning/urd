#pragma once

#include "urd/interface.hpp"
#include "urd/properties.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/core/dependent_false.hpp"
#include "urd/repr/dense.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/entry_ordered.hpp"
#include "urd/repr/flat.hpp"
#include "urd/repr/flat_hash.hpp"
#include "urd/repr/grouped.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/layered.hpp"
#include "urd/repr/ordered_map.hpp"
#if defined(URD_HAS_GTL)
#include "urd/repr/parallel.hpp"
#endif
#include "urd/repr/pools.hpp"
#include "urd/repr/queues.hpp"
#include "urd/repr/records.hpp"
#include "urd/repr/recycling.hpp"
#include "urd/repr/rows.hpp"
#include "urd/repr/scoped.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/slots.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/repr/tree_bitset.hpp"
#include "urd/repr/tree_vector.hpp"
#include "urd/traits.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <ranges>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace urd::detail
{

/// What a representation must guarantee for spec S: S's properties, except where the Store provides them.
/// A frozen Store's elements never move whatever the representation; a sequence's positions are dense
/// in every representation of a sequence.
template<typename S>
inline constexpr property_set requirement = []
{
    auto result = S::urd_properties;
    if (result.has(tag::frozen))
        result.put(tag::moving_refs);
    if (S::is_sequence)
        result.put(tag::any_keys);
    return result;
}();
/// Whether representation R can serve spec S: it declares its properties and has S's for its elements (a set
/// of elements without operator< is not sorted), and the interface a Store for S offers: it places elements as
/// S does (by value or where they are put), demands nothing S left open (a dense container needs the keys it was
/// promised), is unique exactly where S is (a unique container would drop a
/// sequence's duplicates), is reached by handle exactly where S is (a pool offers no iteration), stores records flat
/// exactly where S asks (records come back by index), is a map exactly
/// where S is, is a queue exactly where S is, and is random-access where S
/// is a sequence. A sequence
/// kept by value must be sorted, or its positions would mean nothing. Where S's elements (a map's values) are Stores (rows), R
/// holds them with their requested properties (element_traits).
template<typename R, typename S>
concept serves_by_traits = container_traits<R>::declared && detail::satisfies(container_traits<R>::urd_properties, requirement<S>)
                           && detail::demands_met(container_traits<R>::urd_properties, S::urd_properties) && by_value<container_traits<R>> == by_value<S>
                           && has<container_traits<R>, unique> == has<S, unique> && has<container_traits<R>, handles> == has<S, handles>
                           && has<container_traits<R>, flat> == has<S, flat> && (!S::is_sequence || !by_value<S> || has<S, sorted>)
                           && queue_traits<container_traits<R>> == S::is_queue;
// Selection reads only the traits, which are partial specializations matched on R's arguments: checking them
// instantiates no container. Whether a container offers what its traits promise (conforming) is checked once, by the
// typed test suites, for every registered container, not again for every request: those interface checks were most
// of what resolving a request cost the compiler. Positions are the exception, checked where asked for.
/// What S holds as rows where they are Stores: a map's values, else its elements.
template<typename S>
using row_of = std::conditional_t<S::is_map, typename S::mapped_type, typename S::element_type>;

template<typename R, typename S>
concept serves = serves_by_traits<R, S> && detail::promises(S::urd_properties, relies_on<R>) && (!has<S, positional> || std::ranges::random_access_range<R>)
                 && (!is_store<row_of<S>> || detail::satisfies(element_traits<R>::urd_properties, requirement<container_traits<row_of<S>>>));

// The containers, with urd's hash, equality and order, so that any hashable or orderable element works and nothing
// has to be specialized in namespace std.
template<typename T>
using HashSet = std::unordered_set<T, Hash<T>, EqualTo<T>>;
template<typename T>
using FlatHashSet = boost::unordered_flat_set<T, Hash<T>, EqualTo<T>>;
template<typename T>
using TreeSet = std::set<T, Less<T>>;
template<typename T>
using FlatSet = boost::container::flat_set<T, Less<T>>;
template<typename T>
using FlatMultiSet = boost::container::flat_multiset<T, Less<T>>;

template<typename K, typename V>
using HashMapOf = std::unordered_map<K, V, Hash<K>, EqualTo<K>>;
template<typename K, typename V>
using HashMultiMapOf = std::unordered_multimap<K, V, Hash<K>, EqualTo<K>>;
template<typename K, typename V>
using FlatHashMapOf = boost::unordered_flat_map<K, V, Hash<K>, EqualTo<K>>;
template<typename K, typename V>
using TreeMapOf = std::map<K, V, Less<K>>;
template<typename K, typename V>
using TreeMultiMapOf = std::multimap<K, V, Less<K>>;
template<typename K, typename V>
using FlatMapOf = boost::container::flat_map<K, V, Less<K>>;
template<typename K, typename V>
using FlatMultiMapOf = boost::container::flat_multimap<K, V, Less<K>>;

template<typename K, typename V>
using DenseMapOf = repr::IndexMap<K, V, false>;
template<typename K, typename V>
using StableDenseMapOf = repr::IndexMap<K, V, false, false, true>;
template<typename K, typename V>
using GenerationMapOf = repr::IndexMap<K, V, true>;
template<typename K, typename V>
using StableGenerationMapOf = repr::IndexMap<K, V, true, false, true>;
template<typename K, typename V>
using ThreadSafeGenerationMapOf = repr::IndexMap<K, V, true, true>;

template<typename T>
using DenseSetOf = repr::DenseSet<T>;
template<typename T>
using TrackedDenseSet = repr::DenseSet<T, true>;
template<typename T>
using SparseSetOf = repr::SparseSet<T>;
template<typename T>
using OrderedSparseSet = repr::SparseSet<T, true>;
template<typename K, typename V>
using OrderedMapOf = repr::OrderedMap<repr::IndexedSet<K>, V>;
template<typename K, typename V>
using DenseOrderedMapOf = repr::OrderedMap<repr::SparseSet<K, true>, V>;
template<typename K, typename V>
using PostingMapOf = repr::PostingMap<K, V>;
template<typename K, typename V>
using CsrMultiMapOf = repr::CsrMultiMap<K, V>;
template<typename K, typename V>
using BidirectionalCsrOf = repr::BidirectionalCsr<K, V>;

template<typename K, typename V>
using ScopedMapOf = repr::ScopedMap<K, V>;
template<typename T>
using SlotVector = repr::SlotVector<T>;
template<typename T>
using ThreadSafeSlotVector = repr::SlotVector<T, true>;
template<typename Row>
using TreeVectorOf = repr::TreeVectorSet<Row>;
template<typename Row>
using TreeBitsetOf = repr::TreeBitsetSet<Row>;
template<typename Row>
using RowInternerOf = repr::RowInterner<Row>;
template<typename Row>
using ThreadSafeRowInterner = repr::RowInterner<Row, true>;
template<typename Row>
using ThreadSafeTreeVectorOf = repr::TreeVectorSet<Row, 4, true>;
template<typename Row>
using ThreadSafeTreeBitsetOf = repr::TreeBitsetSet<Row, 4, true>;
template<typename T>
using ConcurrentIndexedSet = repr::ConcurrentIndexedSet<T>;
template<typename T>
using IndexedSetOf = repr::IndexedSet<T>;
template<typename T>
using StableIndexedSet = repr::StableIndexedSet<T>;
template<typename Inner>
using JaggedOf = repr::Jagged<Inner>;
template<typename Inner>
using TrackedJaggedOf = repr::Jagged<Inner, true>;
template<typename T>
using BucketQueueOf = repr::BucketQueue<T>;
template<typename T>
using AddressableHeapOf = repr::AddressableHeap<T>;
// gtl's sharded tables, where the build has gtl (URD_WITH_GTL): without them a concurrent set or map by hashed keys
// has no representation.
#if defined(URD_HAS_GTL)
template<typename T>
using ParallelHashSetOf = repr::ParallelHashSet<T>;
template<typename K, typename V>
using ParallelHashMapOf = repr::ParallelHashMap<K, V>;
template<typename T>
using ParallelNodeHashSetOf = repr::ParallelNodeHashSet<T>;
template<typename K, typename V>
using ParallelNodeHashMapOf = repr::ParallelNodeHashMap<K, V>;
#endif
template<typename K, typename V>
using RecyclingHashMapOf = repr::RecyclingHashMap<K, V>;
template<typename K, typename V>
using RecyclingMapOf = repr::RecyclingMap<K, V>;
template<typename T>
using ChangeLoggedHashSet = repr::ChangeLogged<FlatHashSet<T>>;
template<typename T>
using LayeredHashSet = repr::Layered<FlatHashSet<T>>;
template<typename K, typename V>
using LayeredHashMapOf = repr::Layered<FlatHashMapOf<K, V>>;
template<typename K, typename V>
using LayeredDenseMapOf = repr::Layered<DenseMapOf<K, V>>;

template<typename T>
using SegmentedVector = repr::SegmentedVector<T>;

template<typename T>
using RecordInternerOf = repr::RecordInterner<T>;
template<typename T>
using StableRecordInterner = repr::RecordInterner<T, false, true>;
template<typename T>
using ThreadSafeRecordInterner = repr::RecordInterner<T, true>;
template<typename T>
using ArenaInternerOf = repr::ArenaInterner<T>;
template<typename T>
using ThreadSafeArenaInterner = repr::ArenaInterner<T, true>;
template<typename Row>
using LayeredRowInternerOf = repr::LayeredPool<repr::RowInterner<Row>>;
template<typename Row>
using ThreadSafeLayeredRowInterner = repr::LayeredPool<repr::RowInterner<Row, true>>;
template<typename T>
using LayeredRecordInternerOf = repr::LayeredPool<repr::RecordInterner<T>>;
template<typename T>
using StableLayeredRecordInterner = repr::LayeredPool<repr::RecordInterner<T, false, true>>;
template<typename T>
using ThreadSafeLayeredRecordInterner = repr::LayeredPool<repr::RecordInterner<T, true>>;
template<typename T>
using LayeredArenaInternerOf = repr::LayeredPool<repr::ArenaInterner<T>>;
template<typename T>
using ThreadSafeLayeredArenaInterner = repr::LayeredPool<repr::ArenaInterner<T, true>>;
template<typename T>
using ConcurrentSegmentedVector = repr::SegmentedVector<T, repr::default_first_segment_size, true>;

/// Container templates over the element type (sets, sequences, rows), in order of preference.
template<template<typename...> class... Cs>
struct element_candidates
{
};
/// Container templates over the key and the value type (maps), in order of preference.
template<template<typename, typename> class... Cs>
struct map_candidates
{
};

/// Every representation, in order of preference; the first that serves a request is chosen. A container is
/// registered by adding it to one of these lists; what it can serve, its container_traits say. The order
/// follows docs/BENCHMARKS.md: an open-addressing hash table is fastest at insert, lookup and clear-and-refill
/// at every size, so it comes first wherever order is not asked for; node tables and segments remain for stable_refs;
/// where order is asked for, a tree serves elements inserted over time. The dense containers come after the hash
/// tables, and only a request that promises dense keys reaches them at all (demands_met: a container that stores by
/// index demands keys that are indices, whatever its place in the list); the indexed set, after them too, only a
/// request for insertion order or positions. Frozen rows are flattened first; rows
/// are kept for reuse only where their memory must be.
using element_preference = element_candidates<JaggedOf,
                                              TrackedJaggedOf,
                                              std::vector,
                                              FlatHashSet,
                                              HashSet,
                                              TreeSet,
                                              FlatMultiSet,
                                              FlatSet,
                                              DenseSetOf,
                                              TrackedDenseSet,
                                              SparseSetOf,
                                              IndexedSetOf,
                                              StableIndexedSet,
                                              OrderedSparseSet,
                                              SegmentedVector,
                                              ConcurrentSegmentedVector,
                                              repr::RowPool,
                                              repr::Fifo,
                                              repr::Lifo,
                                              BucketQueueOf,
                                              repr::Heap,
                                              AddressableHeapOf,
                                              LayeredHashSet,
                                              ChangeLoggedHashSet,
                                              RowInternerOf,
                                              TreeVectorOf,
                                              TreeBitsetOf,
                                              ThreadSafeRowInterner,
                                              ThreadSafeTreeVectorOf,
                                              ThreadSafeTreeBitsetOf,
                                              RecordInternerOf,
                                              ArenaInternerOf,
                                              StableRecordInterner,
                                              ThreadSafeRecordInterner,
                                              ThreadSafeArenaInterner,
                                              LayeredRowInternerOf,
                                              LayeredRecordInternerOf,
                                              LayeredArenaInternerOf,
                                              StableLayeredRecordInterner,
                                              ThreadSafeLayeredRowInterner,
                                              ThreadSafeLayeredRecordInterner,
                                              ThreadSafeLayeredArenaInterner,
#if defined(URD_HAS_GTL)
                                              ParallelHashSetOf,
                                              ParallelNodeHashSetOf,
#endif
                                              ConcurrentIndexedSet,
                                              SlotVector,
                                              ThreadSafeSlotVector,
                                              repr::DisjointSets>;
using map_preference = map_candidates<FlatHashMapOf,
                                      HashMapOf,
                                      PostingMapOf,
                                      HashMultiMapOf,
#if defined(URD_HAS_GTL)
                                      ParallelHashMapOf,
                                      ParallelNodeHashMapOf,
#endif
                                      TreeMapOf,
                                      TreeMultiMapOf,
                                      FlatMultiMapOf,
                                      FlatMapOf,
                                      RecyclingHashMapOf,
                                      RecyclingMapOf,
                                      DenseMapOf,
                                      StableDenseMapOf,
                                      GenerationMapOf,
                                      StableGenerationMapOf,
                                      ThreadSafeGenerationMapOf,
                                      OrderedMapOf,
                                      DenseOrderedMapOf,
                                      CsrMultiMapOf,
                                      BidirectionalCsrOf,
                                      ScopedMapOf,
                                      LayeredHashMapOf,
                                      LayeredDenseMapOf>;

/// The containers that take a comparator, in order of preference (as in the lists above): where a request gives
/// order_by, only they can serve it, instantiated with its comparator.
template<template<typename, typename> class... Cs>
struct ordered_element_candidates
{
};
template<template<typename, typename, typename> class... Cs>
struct ordered_map_candidates
{
};
template<typename T, typename C>
using TreeSetBy = std::set<T, C>;
template<typename T, typename C>
using FlatSetBy = boost::container::flat_set<T, C>;
template<typename T, typename C>
using FlatMultiSetBy = boost::container::flat_multiset<T, C>;
template<typename T, typename C>
using HeapBy = repr::Queue<T, repr::leaving::least, C>;
template<typename T, typename C>
using BucketQueueBy = repr::BucketQueue<T, C>;
template<typename T, typename C>
using AddressableHeapBy = repr::AddressableHeap<T, C>;
template<typename K, typename V, typename C>
using TreeMapBy = std::map<K, V, C>;
template<typename K, typename V, typename C>
using TreeMultiMapBy = std::multimap<K, V, C>;
template<typename K, typename V, typename C>
using FlatMapBy = boost::container::flat_map<K, V, C>;
template<typename K, typename V, typename C>
using FlatMultiMapBy = boost::container::flat_multimap<K, V, C>;
template<typename K, typename V, typename C>
using RecyclingMapBy = repr::Recycling<std::map<K, V, C>>;
using ordered_element_preference = ordered_element_candidates<TreeSetBy, FlatMultiSetBy, FlatSetBy, BucketQueueBy, HeapBy, AddressableHeapBy>;
using ordered_map_preference = ordered_map_candidates<TreeMapBy, TreeMultiMapBy, FlatMultiMapBy, FlatMapBy, RecyclingMapBy>;
/// Maps ordered by their values too (order_by<P> with P(key, value)).
template<typename K, typename V, typename C>
using FlatEntryMapBy = repr::FlatEntryMap<K, V, C>;
using value_ordered_map_preference = ordered_map_candidates<FlatEntryMapBy>;

template<typename... Lists>
struct join
{
    using type = type_list<>;
};
template<typename... Ts>
struct join<type_list<Ts...>>
{
    using type = type_list<Ts...>;
};
template<typename... As, typename... Bs, typename... Lists>
struct join<type_list<As...>, type_list<Bs...>, Lists...> : join<type_list<As..., Bs...>, Lists...>
{
};

/// The registered containers for spec S, instantiated with its element type, or a map's key and value types.
template<typename S, typename Candidates>
struct instantiated;
template<typename S, template<typename...> class... Cs>
struct instantiated<S, element_candidates<Cs...>>
{
    using type = type_list<Cs<typename S::element_type>...>;
};
template<typename S, template<typename, typename> class... Cs>
struct instantiated<S, map_candidates<Cs...>>
{
    using type = type_list<Cs<typename S::key_type, typename S::mapped_type>...>;
};
template<typename S, template<typename, typename> class... Cs>
struct instantiated<S, ordered_element_candidates<Cs...>>
{
    using type = type_list<Cs<typename S::element_type, typename S::order_type>...>;
};
template<typename S, template<typename, typename, typename> class... Cs>
struct instantiated<S, ordered_map_candidates<Cs...>>
{
    using type = type_list<Cs<typename S::key_type, typename S::mapped_type, typename S::order_type>...>;
};

/// Stable storage: it keeps references stable, by a node per element or by segments that never move. Under
/// small or frozen these move last, as contiguous storage is faster for few or unchanging elements.
template<typename R>
inline constexpr bool stable_storage = has<container_traits<R>, stable_refs>;

/// The order for S: where elements are few or never change, containers with stable storage come last, since contiguous
/// storage is more compact and faster to search and iterate (docs/BENCHMARKS.md); otherwise the lists' order.
template<typename S, typename List>
struct ordered_for;
template<typename S, typename... Rs>
struct ordered_for<S, type_list<Rs...>>
{
    static constexpr bool compact = has<S, frozen> || has<S, small>;
    // The reordered list is built only where it is used: join is the costly part of choosing.
    using type = typename std::conditional_t<
        compact,
        join<std::conditional_t<stable_storage<Rs>, type_list<>, type_list<Rs>>..., std::conditional_t<stable_storage<Rs>, type_list<Rs>, type_list<>>...>,
        std::type_identity<type_list<Rs...>>>::type;
};

/// The container chosen.
template<typename R>
struct chosen
{
    static constexpr bool found = true;
    using type = R;
};

/// The first of Rs that serves S, checked one by one and no further: of each conditional only the branch taken is
/// instantiated. Where none serves, a stand-in after the static_assert in choose_in, so that it is the only error.
template<typename S, typename... Rs>
struct first_serving
{
    static constexpr bool found = false;
    using type = std::vector<typename S::element_type>;
};
template<typename S, typename R, typename... Rs>
struct first_serving<S, R, Rs...> : std::conditional_t<serves<R, S>, chosen<R>, first_serving<S, Rs...>>
{
};

template<typename S, typename List>
struct choose_in;

/// The first of the containers that serves S, if any (found).
template<typename S, typename... Rs>
struct choose_in<S, type_list<Rs...>> : first_serving<S, Rs...>
{
};

/// The candidates for spec S: the comparator-taking ones where it gives order_by (those ordered by values too where
/// its P reads them), else all.
template<typename S>
using candidates_for = std::conditional_t<
    std::is_void_v<typename S::order_type>,
    std::conditional_t<S::is_map, instantiated<S, map_preference>, instantiated<S, element_preference>>,
    std::conditional_t<
        S::is_map,
        std::conditional_t<is_entry_less<typename S::order_type>, instantiated<S, value_ordered_map_preference>, instantiated<S, ordered_map_preference>>,
        instantiated<S, ordered_element_preference>>>;

/// The representation for spec S: the first registered container that serves it, in the order for S.
template<typename S>
using choose = choose_in<S, typename ordered_for<S, typename candidates_for<S>::type>::type>;

/// The tags of an unservable request whose removal, one at a time, would make it servable: what to drop, or what the
/// pool lacks a representation for. Named in the error for such a request.
template<tag... Ts>
struct culprits
{
};

/// The static assertion a request no representation serves fails with. Its arguments say why: the spec, the culprit
/// tags, and whether the key (a sequence's or queue's element) is hashable, orderable and indexable, for when no tag
/// is the culprit but the type is.
template<typename S, typename Culprits, bool KeyHashable, bool KeyOrderable, bool KeyIndexable>
struct no_representation_for
{
#if !defined(URD_HAS_GTL)
    static_assert(!has<S, concurrent> || S::is_sequence || S::is_pool,
                  "urd::Store: a concurrent set or map by hashed keys is gtl's sharded table; this build has URD_WITH_GTL=OFF");
#endif
    static_assert(repr::dependent_false<S>::value,
                  "urd::Store: no representation provides this combination of properties. The culprits<...> above name the tags "
                  "whose removal, one at a time, would make it servable: drop one, or add a representation for them. No "
                  "culprit named: the key type lacks what the request needs (hashable, orderable, indexable: the booleans "
                  "above), or two of the tags clash");
    /// A stand-in, so that the assertion is the only error.
    using type = std::vector<typename S::element_type>;
};

template<typename Request, std::size_t... I>
consteval listed_tags culprits_of(std::index_sequence<I...> /*positions*/)
{
    auto result = listed_tags {};
    ((choose<typename Request::template without<Request::stated_tags.tags[I]>>::found ? void(result.tags[result.count++] = Request::stated_tags.tags[I]) :
                                                                                        void()),
     ...);
    return result;
}
template<typename Request, listed_tags Found, std::size_t... I>
consteval auto diagnosis(std::index_sequence<I...> /*positions*/)
{
    using spec = typename Request::type;
    using probe = std::conditional_t<std::is_void_v<typename spec::key_type>, typename spec::element_type, typename spec::key_type>;
    return std::type_identity<no_representation_for<spec, culprits<Found.tags[I]...>, hashable<probe>, orderable<probe>, indexable<probe>>> {};
}
/// The diagnosis of a request (a to_spec) no representation serves; its type is reached only then.
template<typename Request>
struct unservable
{
    static constexpr listed_tags found = culprits_of<Request>(std::make_index_sequence<Request::stated_tags.count> {});
    using type = typename decltype(diagnosis<Request, found>(std::make_index_sequence<found.count> {}))::type::type;
};

/// The representation for a request (a to_spec), or where none serves it, the diagnosis: the culprit probes run only
/// then.
template<typename Request>
struct resolution
{
    using spec = typename Request::type;
    using type = typename std::conditional_t<choose<spec>::found, choose<spec>, unservable<Request>>::type;
};

}  // namespace urd::detail
