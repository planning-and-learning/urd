#pragma once

#include "urd/traits.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <vector>

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

/// How long each row of a fixed_shape Store is, in order: what such a Store is built from.
struct row_lengths
{
    std::vector<std::size_t> lengths;

    row_lengths(std::initializer_list<std::size_t> each) : lengths(each) {}
    explicit row_lengths(std::vector<std::size_t> each) : lengths(std::move(each)) {}
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

/// Whether P is a tag a Store may be asked for. Mutability is not one: it follows from frozen and monotone.
template<typename P>
consteval bool is_tag()
{
    if constexpr (a_tag<P>)
        return (is_guarantee<P> && root_of(P::value) != tag::immutable) || is_request_only<P>;
    else
        return false;
}

template<typename P>
inline constexpr bool is_property = is_tag<P>() || is_a<key, P> || is_a<value, P>;

/// P's number, where P is a tag; tag::count for key<K> and value<V>.
template<typename P>
consteval tag tag_of()
{
    if constexpr (a_tag<P>)
        return P::value;
    else
        return tag::count;
}

}  // namespace detail

/// A Store's request in canonical form: its key and value types (void where absent), and the tags that say more than
/// their category's root, in the order of urd::tag. Whatever order the properties were listed in, equal requests give
/// equal specs, and so equal Store types; compilers print the tags by name.
template<typename K, typename V, tag... Tags>
struct spec
{
    using key_type = K;
    using value_type = V;

    static constexpr detail::property_set urd_properties = detail::property_set::of({ Tags... });
    static constexpr bool declared = true;

    /// Keys grouped into disjoint classes (partition), reached by merge and find.
    static constexpr bool is_partition = !std::is_void_v<K> && std::is_void_v<V> && urd_properties.in(tag::any_grouping) == tag::partition;
    static constexpr bool is_set = !std::is_void_v<K> && std::is_void_v<V> && !is_partition;
    static constexpr bool is_map = !std::is_void_v<K> && !std::is_void_v<V>;
    /// Values reached by the order they leave in (fifo, lifo, by_priority), not by position.
    static constexpr bool is_queue = std::is_void_v<K> && !std::is_void_v<V> && urd_properties.in(tag::any_access) != tag::any_access;
    /// Values identified by the handles insert gives (handles): a pool.
    static constexpr bool is_pool = std::is_void_v<K> && !std::is_void_v<V> && urd_properties.in(tag::any_handle) == tag::handles;
    static constexpr bool is_sequence = std::is_void_v<K> && !std::is_void_v<V> && !is_queue && !is_pool;
    /// Whether its elements can be visited in turn: all but a queue, which is reached by its front or top. A filter,
    /// which only answers membership, would not be either.
    static constexpr bool iterable = is_set || is_map || is_sequence;
    /// Whether it knows how many elements it holds: all but an interning pool (handles, unique), which may share
    /// parts of its elements and keeps no list of them. A filter would not either.
    static constexpr bool sized = is_set || is_map || is_sequence || is_queue || is_partition || (is_pool && !urd_properties.has(tag::unique));

    /// What a key maps to, in a map.
    using mapped_type = std::conditional_t<is_map, V, void>;
    /// What a Store's elements are: keys in a set, values in a sequence, key-value pairs in a map.
    using element_type = std::conditional_t<is_set || is_partition, K, std::conditional_t<is_map, std::pair<const K, V>, V>>;
};

namespace detail
{

/// The tags of a list (key<K> and value<V> skipped), as stated.
consteval property_set stated_of(std::initializer_list<tag> listed)
{
    auto result = property_set::roots();
    for (const auto each : listed)
        if (each != tag::count)
            result.put(each);
    return result;
}

/// Whether at most one listed tag answers each category.
consteval bool one_per_category(std::initializer_list<tag> listed)
{
    auto answers = std::array<int, tag_count> {};
    for (const auto each : listed)
        if (each != tag::count && ++answers[index(root_of(each))] > 1)
            return false;
    return true;
}

/// What a request's kind is, from its key and value types and the categories that make a kind.
struct kind
{
    bool set = false;
    bool map = false;
    bool sequence = false;
    bool queue = false;
};

/// The request rules: what the list implies, in place of what it states.
consteval property_set implied(property_set stated, kind of)
{
    auto result = stated;
    // A sequence is positional; a set may be too, with its elements in insertion order unless it asks for another
    // (an interner: each element's position is its index).
    if (of.sequence)
        result.put(tag::positional);
    if (of.set && stated.has(tag::positional) && !stated.has(tag::sorted))
        result.put(tag::insertion_ordered);
    // A set's keys are its contents, each once; a map's keys are each once unless it is a multimap.
    if (of.set || (of.map && !stated.has(tag::multi)))
        result.put(tag::unique);
    // A map has one value per key unless it says multi; a sequence one value per position.
    if ((of.map && !stated.has(tag::multi)) || of.sequence)
        result.put(tag::functional);
    // Positions are 0..n-1.
    if (of.sequence)
        result.put(tag::dense);
    // Nothing moves once nothing changes.
    if (stated.has(tag::frozen))
        result.put(tag::stable_refs);
    // What the representation must offer: nothing if frozen, inserts if it only grows or must stay nonempty; a queue
    // offers push and pop whenever it may change, clear too unless it is monotone.
    if (stated.has(tag::frozen) || stated.has(tag::fixed_shape))
        result.put(tag::immutable);
    else if (stated.has(tag::monotone) || stated.has(tag::nonempty))
        result.put(tag::growable);
    else
        result.put(tag::shrinkable);
    return result;
}

/// The tags of a set that say more than their category's root, in the order of urd::tag: a spec's arguments.
struct listed_tags
{
    std::array<tag, tag_count> tags {};
    std::size_t count = 0;
};
consteval listed_tags non_roots(const property_set& properties)
{
    auto result = listed_tags {};
    for (std::size_t each = 0; each < tag_count; ++each)
    {
        const auto root = static_cast<tag>(each);
        if (root_of(root) == root && properties.in(root) != root)
            result.tags[result.count++] = properties.in(root);
    }
    return result;
}
template<typename K, typename V, listed_tags Listed, std::size_t... I>
spec<K, V, Listed.tags[I]...> spec_of(std::index_sequence<I...> /*positions*/);

/// Checks a property list and turns it into its spec. Each static_assert names what is wrong.
template<typename... Ps>
struct to_spec
{
    static_assert((is_property<Ps> && ...),
                  "urd::Store: every argument must be an urd property (key<K>, value<V>, sorted, unique, …, or a "
                  "workload such as small)");
    static_assert((0 + ... + static_cast<int>(is_a<key, Ps>)) <= 1, "urd::Store: at most one key<K>");
    static_assert((0 + ... + static_cast<int>(is_a<value, Ps>)) <= 1, "urd::Store: at most one value<V>");
    static_assert(one_per_category({ tag_of<Ps>()... }),
                  "urd::Store: two properties answer the same question (dense and sparse, functional and multi, "
                  "frozen and monotone, two workloads, …)");

    using key_type = typename first_of<key, Ps...>::type;
    using value_type = typename first_of<value, Ps...>::type;
    static_assert(!std::is_void_v<key_type> || !std::is_void_v<value_type>, "urd::Store: needs key<K> (a set or map) or value<V> (a sequence)");

    static constexpr property_set stated = stated_of({ tag_of<Ps>()... });
    static constexpr bool keyed = !std::is_void_v<key_type>;
    static constexpr bool valued = !std::is_void_v<value_type>;
    static constexpr bool queue = !keyed && valued && stated.in(tag::any_access) != tag::any_access;
    static constexpr bool pool = !keyed && valued && stated.has(tag::handles);
    static constexpr kind of { .set = keyed && !valued && !stated.has(tag::partition),
                               .map = keyed && valued,
                               .sequence = !keyed && valued && !queue && !pool,
                               .queue = queue };

    static_assert(!stated.has(tag::partition) || (!valued && !stated.has(tag::frozen)),
                  "urd::Store: a partition groups key<K>s, without values, and changes by insert and merge");
    static_assert(!stated.has(tag::handles) || (!keyed && !queue && !stated.has(tag::positional) && !stated.has(tag::frozen)),
                  "urd::Store: a pool (handles) holds value<V> rows, gains them by insert, and is reached by handle, not by key, "
                  "position or order");
    static_assert(!queue || (!stated.has(tag::frozen) && !stated.has(tag::positional)),
                  "urd::Store: a queue changes by push and pop, and is reached by its front or top, not by position");
    static_assert(!stated.has(tag::fixed_shape) || (of.sequence && !stated.has(tag::monotone) && is_store<value_type>),
                  "urd::Store: fixed_shape is a sequence of rows (value<Store<…>>) whose number and lengths never change; its rows' "
                  "contents do");
    static_assert(!of.map || !stated.has(tag::positional), "urd::Store: positional identifies a map's elements by position, so it cannot have key<K>");
    static_assert(!(stated.has(tag::contiguous) && stated.has(tag::stable_refs)) || stated.has(tag::frozen),
                  "urd::Store: contiguous storage moves its elements when it grows, so stable_refs needs frozen too");

    static constexpr listed_tags listed = non_roots(implied(stated, of));
    using type = decltype(detail::spec_of<key_type, value_type, listed>(std::make_index_sequence<listed.count> {}));
};

}  // namespace detail

}  // namespace urd
