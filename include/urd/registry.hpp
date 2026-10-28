#pragma once

#include "urd/interface.hpp"
#include "urd/properties.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/dense.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/flat.hpp"
#include "urd/repr/flat_hash.hpp"
#include "urd/repr/grouped.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/layered.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/pools.hpp"
#include "urd/repr/queues.hpp"
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
/// S does (by value or where they are put), is unique exactly where S is (a unique container would drop a
/// sequence's duplicates), is a map exactly where S is, and is random-access where S is a sequence. A sequence
/// kept by value must be sorted, or its positions would mean nothing. Where S's elements are Stores (rows), R
/// holds them with their requested properties (element_traits).
template<typename R, typename S>
concept serves_by_traits =
    container_traits<R>::declared && detail::satisfies(container_traits<R>::urd_properties, requirement<S>) && by_value<container_traits<R>> == by_value<S>
    && has<container_traits<R>, unique> == has<S, unique> && (!S::is_sequence || !by_value<S> || has<S, sorted>);
// Selection reads only the traits, which are partial specializations matched on R's arguments: checking them
// instantiates no container. Whether a container offers what its traits promise (conforming) is checked once, by the
// typed test suites, for every registered container, not again for every request: those interface checks were most
// of what resolving a request cost the compiler. Positions are the exception, checked where asked for.
template<typename R, typename S>
concept serves =
    serves_by_traits<R, S> && (!has<S, positional> || std::ranges::random_access_range<R>)
    && (!is_store<typename S::element_type> || detail::satisfies(element_traits<R>::urd_properties, requirement<container_traits<typename S::element_type>>));

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
using GenerationMapOf = repr::IndexMap<K, V, true>;
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
using ChangeLoggedHashSet = repr::ChangeLogged<FlatHashSet<T>>;
template<typename T>
using LayeredHashSet = repr::Layered<FlatHashSet<T>>;
template<typename K, typename V>
using LayeredHashMapOf = repr::Layered<FlatHashMapOf<K, V>>;

template<typename T>
using SegmentedVector = repr::SegmentedVector<T>;
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
/// at every size, so it comes first wherever order is not asked for; node-based tables remain for stable_refs;
/// where order is asked for, a tree serves elements inserted over time. The dense containers come after the hash
/// tables, which declare sparse keys, so only a request for dense keys reaches them; the indexed set, after
/// them too, only a request for insertion order or positions. Frozen rows are flattened first; rows
/// are kept for reuse only where their memory must be.
using element_preference = element_candidates<repr::Jagged,
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
                                              OrderedSparseSet,
                                              SegmentedVector,
                                              ConcurrentSegmentedVector,
                                              repr::RowPool,
                                              repr::Fifo,
                                              repr::Lifo,
                                              repr::Heap,
                                              LayeredHashSet,
                                              ChangeLoggedHashSet,
                                              RowInternerOf,
                                              TreeVectorOf,
                                              TreeBitsetOf,
                                              ThreadSafeRowInterner,
                                              ThreadSafeTreeVectorOf,
                                              ThreadSafeTreeBitsetOf,
                                              ConcurrentIndexedSet,
                                              SlotVector,
                                              ThreadSafeSlotVector,
                                              repr::DisjointSets>;
using map_preference = map_candidates<FlatHashMapOf,
                                      HashMapOf,
                                      PostingMapOf,
                                      HashMultiMapOf,
                                      TreeMapOf,
                                      TreeMultiMapOf,
                                      FlatMultiMapOf,
                                      FlatMapOf,
                                      DenseMapOf,
                                      GenerationMapOf,
                                      ThreadSafeGenerationMapOf,
                                      OrderedMapOf,
                                      DenseOrderedMapOf,
                                      CsrMultiMapOf,
                                      BidirectionalCsrOf,
                                      ScopedMapOf,
                                      LayeredHashMapOf>;

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

/// Node-based: it keeps references stable by allocating every element on its own.
template<typename R>
inline constexpr bool node_based = has<container_traits<R>, stable_refs>;

/// The order for S: where elements are few or never change, node-based containers come last, since contiguous
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
        join<std::conditional_t<node_based<Rs>, type_list<>, type_list<Rs>>..., std::conditional_t<node_based<Rs>, type_list<Rs>, type_list<>>...>,
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

/// The first of the containers that serves S.
template<typename S, typename... Rs>
struct choose_in<S, type_list<Rs...>> : first_serving<S, Rs...>
{
    static_assert(first_serving<S, Rs...>::found,
                  "urd::Store: no representation provides this combination of properties; "
                  "the pool needs a new representation for it");
};

/// The representation for spec S: the first registered container that serves it, in the order for S.
template<typename S>
using choose = choose_in<
    S,
    typename ordered_for<S, typename std::conditional_t<S::is_map, instantiated<S, map_preference>, instantiated<S, element_preference>>::type>::type>;

}  // namespace urd::detail
