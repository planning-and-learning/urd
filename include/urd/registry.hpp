#pragma once

#include "urd/interface.hpp"
#include "urd/properties.hpp"
#include "urd/repr/flat.hpp"
#include "urd/repr/flat_hash.hpp"
#include "urd/repr/rows.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
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
/// sequence's duplicates), is a map exactly where S is, and is random-access where S is a sequence. Where S's elements are Stores (rows), R
/// holds them with their requested properties (element_traits).
template<typename R, typename S>
concept serves = container_traits<R>::declared && detail::map_container<R> == S::is_map
                 && satisfies<container_traits<R>, requirement<S>> && conforming<R> && by_value<container_traits<R>> == by_value<S>
                 && has<container_traits<R>, unique> == has<S, unique> && (!has<S, positional> || std::ranges::random_access_range<R>)
                 && (!is_store<typename S::element_type> || satisfies<element_traits<R>, requirement<container_traits<typename S::element_type>>>);

/// Container templates, each taking the element type.
template<template<typename...> class... Cs>
struct candidates
{
};

/// No container: what a map candidate is for elements that are not key-value pairs. It declares nothing,
/// so it serves nothing.
struct none
{
};

/// Map M from a Store's element type, std::pair<const K, V>; none for any other element type.
template<template<typename...> class M, typename E>
struct as_map
{
    using type = none;
};
template<template<typename...> class M, typename K, typename V>
struct as_map<M, std::pair<const K, V>>
{
    using type = M<K, V>;
};

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

template<typename E>
using FlatHashMap = typename as_map<FlatHashMapOf, E>::type;
template<typename E>
using Map = typename as_map<TreeMapOf, E>::type;
template<typename E>
using MultiMap = typename as_map<TreeMultiMapOf, E>::type;
template<typename E>
using UnorderedMap = typename as_map<HashMapOf, E>::type;
template<typename E>
using UnorderedMultiMap = typename as_map<HashMultiMapOf, E>::type;
template<typename E>
using FlatMap = typename as_map<FlatMapOf, E>::type;
template<typename E>
using FlatMultiMap = typename as_map<FlatMultiMapOf, E>::type;

template<typename T>
using SegmentedVector = repr::SegmentedVector<T>;
template<typename T>
using ConcurrentSegmentedVector = repr::SegmentedVector<T, repr::default_first_segment_size, true>;

/// Every representation, in order of preference for S; the first that serves S is chosen. The orders follow
/// docs/BENCHMARKS.md: an open-addressing hash table is fastest at insert, lookup and clear-and-refill at every
/// size, so it comes first wherever order is not asked for; node-based tables remain for stable_refs. Where
/// order is asked for, a tree serves elements inserted over time, and a sorted vector (faster to search and
/// iterate, O(n) to insert) serves elements that are few or never change. Frozen rows are flattened first;
/// rows are kept for reuse only where their memory must be. A container is registered by adding it to both
/// lists; what it can serve, its container_traits say.
template<typename S>
using preference = std::conditional_t<has<S, frozen> || has<S, small>,
                                      candidates<repr::Jagged,
                                                 std::vector,
                                                 FlatMultiSet,
                                                 FlatHashSet,
                                                 FlatSet,
                                                 TreeSet,
                                                 HashSet,
                                                 FlatMultiMap,
                                                 FlatHashMap,
                                                 FlatMap,
                                                 Map,
                                                 MultiMap,
                                                 UnorderedMap,
                                                 UnorderedMultiMap,
                                                 SegmentedVector,
                                                 ConcurrentSegmentedVector,
                                                 repr::RowPool>,
                                      candidates<repr::Jagged,
                                                 std::vector,
                                                 FlatHashSet,
                                                 HashSet,
                                                 TreeSet,
                                                 FlatMultiSet,
                                                 FlatSet,
                                                 FlatHashMap,
                                                 UnorderedMap,
                                                 UnorderedMultiMap,
                                                 Map,
                                                 MultiMap,
                                                 FlatMultiMap,
                                                 FlatMap,
                                                 SegmentedVector,
                                                 ConcurrentSegmentedVector,
                                                 repr::RowPool>>;

template<typename S, typename Candidates = preference<S>>
struct choose;

/// The representation for spec S: the first of the candidates that serves S.
template<typename S, template<typename...> class... Cs>
struct choose<S, candidates<Cs...>>
{
    using element = typename S::element_type;
    static constexpr std::size_t count = sizeof...(Cs);
    static constexpr std::array<bool, count> fits { serves<Cs<element>, S>... };
    static constexpr std::size_t best = []
    {
        std::size_t found = 0;
        while (found < count && !fits[found])
            ++found;
        return found;
    }();

    static_assert(best < count,
                  "urd::Store: no representation provides this combination of properties; "
                  "the pool needs a new representation for it");

    using type = std::tuple_element_t < best<count ? best : 0, std::tuple<Cs<element>...>>;
};

}  // namespace urd::detail
