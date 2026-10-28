#pragma once

#include "urd/interface.hpp"
#include "urd/properties.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/csr.hpp"
#include "urd/repr/dense.hpp"
#include "urd/repr/flat.hpp"
#include "urd/repr/flat_hash.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/layered.hpp"
#include "urd/repr/pools.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/queues.hpp"
#include "urd/repr/rows.hpp"
#include "urd/repr/scoped.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
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
using requirement = with_t<S,
                           std::conditional_t<has<S, frozen>, moving_refs, category_t<S, moving_refs>>,
                           std::conditional_t<S::is_sequence, any_keys, category_t<S, any_keys>>>;
/// Whether representation R can serve spec S: it declares its properties and has S's for its elements (a set
/// of elements without operator< is not sorted), and the interface a Store for S offers: it places elements as
/// S does (by value or where they are put), is unique exactly where S is (a unique container would drop a
/// sequence's duplicates), is a map exactly where S is, and is random-access where S is a sequence. A sequence
/// kept by value must be sorted, or its positions would mean nothing. Where S's elements are Stores (rows), R
/// holds them with their requested properties (element_traits).
template<typename R, typename S>
concept serves = container_traits<R>::declared && detail::map_container<R> == S::is_map && satisfies<container_traits<R>, requirement<S>> && conforming<R>
                 && by_value<container_traits<R>> == by_value<S> && has<container_traits<R>, unique> == has<S, unique>
                 && (!has<S, positional> || std::ranges::random_access_range<R>) && (!S::is_sequence || !by_value<S> || has<S, sorted>)
                 && (!is_store<typename S::element_type> || satisfies<element_traits<R>, requirement<container_traits<typename S::element_type>>>);

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
using PostingMapOf = repr::PostingMap<K, V>;
template<typename K, typename V>
using CsrMultiMapOf = repr::CsrMultiMap<K, V>;

template<typename K, typename V>
using ScopedMapOf = repr::ScopedMap<K, V>;
template<typename Row>
using TreeVectorOf = repr::TreeVectorSet<Row>;
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
                                              repr::DenseSet,
                                              repr::SparseSet,
                                              repr::IndexedSet,
                                              SegmentedVector,
                                              ConcurrentSegmentedVector,
                                              repr::RowPool,
                                              repr::Fifo,
                                              repr::Lifo,
                                              repr::Heap,
                                              LayeredHashSet,
                                              ChangeLoggedHashSet,
                                              repr::RowInterner,
                                              TreeVectorOf>;
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
                                      CsrMultiMapOf,
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
    using type = std::conditional_t<compact,
                                    typename join<std::conditional_t<node_based<Rs>, type_list<>, type_list<Rs>>...,
                                                  std::conditional_t<node_based<Rs>, type_list<Rs>, type_list<>>...>::type,
                                    type_list<Rs...>>;
};

template<typename S, typename List>
struct choose_in;

/// The first of the containers that serves S.
template<typename S, typename... Rs>
struct choose_in<S, type_list<Rs...>>
{
    static constexpr std::size_t count = sizeof...(Rs);
    static constexpr std::array<bool, count> fits { serves<Rs, S>... };
    static constexpr std::size_t best = []
    {
        std::size_t found = 0;
        while (found < count && !fits.at(found))
            ++found;
        return found;
    }();

    static_assert(best < count,
                  "urd::Store: no representation provides this combination of properties; "
                  "the pool needs a new representation for it");

    // Where none serves, a stand-in after the static_assert, so that it is the only error.
    using type = std::tuple_element_t<best, std::tuple<Rs..., std::vector<typename S::element_type>>>;
};

/// The representation for spec S: the first registered container that serves it, in the order for S.
template<typename S>
using choose = choose_in<
    S,
    typename ordered_for<S, typename std::conditional_t<S::is_map, instantiated<S, map_preference>, instantiated<S, element_preference>>::type>::type>;

}  // namespace urd::detail
