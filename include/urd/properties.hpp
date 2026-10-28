#pragma once

#include "urd/traits.hpp"

#include <type_traits>
#include <utility>

namespace urd
{

// What a Store can be asked for: element types and the property tags of traits.hpp (workloads such as small
// among them). Each is a type, so they can be listed together, in any order.

/// The elements are identified by keys of type K. Without value<V>, the keys are the contents: a set.
template<typename K>
struct key
{
    using type = K;
};

/// Each element carries a value of type V. Without key<K>, values are identified by position: a sequence.
template<typename V>
struct value
{
    using type = V;
};

namespace detail
{

template<template<typename> class Template, typename T>
inline constexpr bool is_a = false;
template<template<typename> class Template, typename T>
inline constexpr bool is_a<Template, Template<T>> = true;

/// The type inside the first Template<…> among Ps, else void.
template<template<typename> class Template, typename... Ps>
struct first_of
{
    using type = void;
};
template<template<typename> class Template, typename T, typename... Ps>
struct first_of<Template, Template<T>, Ps...>
{
    using type = T;
};
template<template<typename> class Template, typename P, typename... Ps>
struct first_of<Template, P, Ps...> : first_of<Template, Ps...>
{
};

/// A tag a Store may be asked for. Mutability is not one: it follows from frozen and monotone.
template<typename T>
inline constexpr bool is_tag = (is_guarantee<T> && !std::derived_from<T, immutable>) || is_request_only<T>;

template<typename T>
inline constexpr bool is_property = is_tag<T> || is_a<key, T> || is_a<value, T>;

}  // namespace detail

/// A Store's request in canonical form: whatever order the properties were listed in, equal requests give
/// equal specs, and so equal Store types. Its categories are those of Properties; K and V (void where absent)
/// make its kind.
template<typename K, typename V, typename Properties>
struct spec : Properties
{
    using key_type = K;
    using value_type = V;

    static constexpr bool is_set = !std::is_void_v<K> && std::is_void_v<V>;
    static constexpr bool is_map = !std::is_void_v<K> && !std::is_void_v<V>;
    static constexpr bool is_sequence = std::is_void_v<K> && !std::is_void_v<V>;
    /// Whether its elements can be visited in turn: every kind so far. A queue, reached by its front, or a filter,
    /// which only answers membership, would not be.
    static constexpr bool iterable = is_set || is_map || is_sequence;
    /// Whether it knows how many elements it holds: every kind so far. A filter would not.
    static constexpr bool sized = is_set || is_map || is_sequence;

    /// What a key maps to, in a map.
    using mapped_type = std::conditional_t<is_map, V, void>;
    /// What a Store's elements are: keys in a set, values in a sequence, key-value pairs in a map.
    using element_type = std::conditional_t<is_set, K, std::conditional_t<is_map, std::pair<const K, V>, V>>;
};

namespace detail
{

/// Checks a property list and turns it into its spec. Each static_assert names what is wrong.
template<typename... Ps>
struct to_spec
{
    static_assert((is_property<Ps> && ...),
                  "urd::Store: every argument must be an urd property (key<K>, value<V>, sorted, unique, …, or a "
                  "workload such as small)");
    static_assert((0 + ... + static_cast<int>(is_a<key, Ps>)) <= 1, "urd::Store: at most one key<K>");
    static_assert((0 + ... + static_cast<int>(is_a<value, Ps>)) <= 1, "urd::Store: at most one value<V>");
    static_assert(one_per_category<guaranteed_roots, Ps...> && one_per_category<requested_roots, Ps...>,
                  "urd::Store: two properties answer the same question (dense and sparse, functional and multi, "
                  "frozen and monotone, two workloads, …)");

    using key_type = typename first_of<key, Ps...>::type;
    using value_type = typename first_of<value, Ps...>::type;
    static_assert(!std::is_void_v<key_type> || !std::is_void_v<value_type>, "urd::Store: needs key<K> (a set or map) or value<V> (a sequence)");

    // The kind, from one place.
    using kind = spec<key_type, value_type, properties_of<>>;

    using stated = properties_of<Ps...>;
    static_assert(!kind::is_map || !has<stated, positional>, "urd::Store: positional identifies a map's elements by position, so it cannot have key<K>");
    static_assert(!has<stated, contiguous, stable_refs> || has<stated, frozen>,
                  "urd::Store: contiguous storage moves its elements when it grows, so stable_refs needs frozen too");

    // What the list implies, in place of what it states.
    // A sequence is positional; a set may be too, with its elements in insertion order unless it asks for another
    // (an interner: each element's position is its index).
    using identity = std::conditional_t<kind::is_sequence, positional, category_t<stated, by_key>>;
    using order = std::conditional_t<kind::is_set && has<stated, positional> && !has<stated, sorted>, insertion_ordered, category_t<stated, any_order>>;
    // A set's keys are its contents, each once; a map's keys are each once unless it is a multimap.
    using uniqueness = std::conditional_t<kind::is_set || (kind::is_map && !has<stated, multi>), unique, category_t<stated, duplicates>>;
    // A map has one value per key unless it says multi; a sequence one value per position.
    using multiplicity = std::conditional_t<(kind::is_map && !has<stated, multi>) || kind::is_sequence, functional, category_t<stated, any_multiplicity>>;
    // Positions are 0..n-1.
    using keys = std::conditional_t<kind::is_sequence, dense, category_t<stated, any_keys>>;
    // Nothing moves once nothing changes.
    using references = std::conditional_t<has<stated, frozen>, stable_refs, category_t<stated, moving_refs>>;
    // What the representation must offer: nothing if frozen, inserts if it only grows or must stay nonempty.
    using mutability =
        std::conditional_t<has<stated, frozen>, immutable, std::conditional_t<has<stated, monotone> || has<stated, nonempty>, growable, shrinkable>>;

    using type = spec<key_type, value_type, with_t<stated, identity, order, uniqueness, multiplicity, keys, references, mutability>>;
};

}  // namespace detail

}  // namespace urd
