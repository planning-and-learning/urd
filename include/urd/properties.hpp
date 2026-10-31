#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd
{

// What a Store can be asked for: element types and the property tags of tags.hpp (workloads such as small
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

/// Orders a sorted Store, or a by_priority queue, by P(element) (P(key) in a map) instead of the element: P is a member
/// pointer (&Job::deadline) or a stateless lambda ([](const Job& job) { return std::tuple(job.deadline, -job.weight); }). Ties fall back
/// to the element's own order where it has one.
template<auto P>
struct order_by
{
    static constexpr auto projection = P;
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

/// Whether a request may state `of`: a tag of a category a container declares or a request states, and not one the
/// library derives (internal_tags). Mutability is not stated: it follows from frozen and monotone.
constexpr bool statable(tag of)
{
    const auto root = root_of(of);
    return ((in(root, guaranteed_roots) && root != tag::immutable) || in(root, requested_roots)) && !in(of, internal_tags);
}

/// Whether P is a tag the library derives and no request states (internal_tags).
template<typename P>
consteval bool is_internal()
{
    if constexpr (a_tag<P>)
        return in(P::value, internal_tags);
    else
        return false;
}

/// The tags stated, with what one implies of another's category: monotone_priority is a priority queue's promise, so
/// it makes the request a priority queue.
consteval property_set stated_with_access(property_set stated)
{
    if (stated.has(tag::monotone_priority) && stated.in(tag::any_access) == tag::any_access)
        stated.put(tag::by_priority);
    return stated;
}

/// Whether P is a tag a Store may be asked for (statable).
template<typename P>
consteval bool is_tag()
{
    if constexpr (a_tag<P>)
        return statable(P::value);
    else
        return false;
}

template<typename P>
inline constexpr bool is_order_by = false;
template<auto P>
inline constexpr bool is_order_by<order_by<P>> = true;

/// The comparator order_by<P> stands for: with one argument, P orders elements T (a map's keys); with two, a map's
/// entries by key K and value V.
template<typename T, auto P>
consteval auto projected_order()
{
    static_assert(std::invocable<decltype(P), const T&>, "urd::Store: order_by<P> takes P(element) (a map's P(key)), or for a map P(key, value); P is neither");
    return std::type_identity<ProjectedLess<T, P>> {};
}
template<typename T, typename K, typename V, auto P>
consteval auto order_from()
{
    if constexpr (!std::is_void_v<K> && !std::is_void_v<V>)
    {
        if constexpr (std::invocable<decltype(P), const K&, const V&>)
            return std::type_identity<EntryLess<K, V, P>> {};
        else
            return projected_order<T, P>();
    }
    else
        return projected_order<T, P>();
}
/// The comparator of the first order_by<P> among Ps, else void.
template<typename T, typename K, typename V, typename... Ps>
struct order_of
{
    using type = void;
};
template<typename T, typename K, typename V, auto P, typename... Ps>
struct order_of<T, K, V, order_by<P>, Ps...>
{
    using type = typename decltype(order_from<T, K, V, P>())::type;
};
template<typename T, typename K, typename V, typename P, typename... Ps>
struct order_of<T, K, V, P, Ps...> : order_of<T, K, V, Ps...>
{
};

template<typename P>
inline constexpr bool is_property = is_tag<P>() || is_a<key, P> || is_a<value, P> || is_order_by<P>;

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
template<typename K, typename V, typename Order, tag... Tags>
struct spec
{
    using key_type = K;
    using value_type = V;
    /// The comparator order_by asked for (urd::ProjectedLess), else void: the elements' own order.
    using order_type = Order;

    static constexpr detail::property_set urd_properties = detail::property_set::of({ Tags... });
    static constexpr bool declared = true;

    /// Keys grouped into disjoint classes (partition), reached by merge and find.
    static constexpr bool is_partition = !std::is_void_v<K> && std::is_void_v<V> && urd_properties.in(tag::any_grouping) == tag::partition;
    static constexpr bool is_set = !std::is_void_v<K> && std::is_void_v<V> && !is_partition;
    static constexpr bool is_map = !std::is_void_v<K> && !std::is_void_v<V>;
    /// Values reached by the order they leave in (fifo, lifo, by_priority), not by position.
    static constexpr bool is_queue = std::is_void_v<K> && !std::is_void_v<V> && urd_properties.in(tag::any_access) != tag::any_access;
    /// Values identified by the handles insert gives (handles): a pool.
    static constexpr bool is_pool = std::is_void_v<K> && !std::is_void_v<V> && urd_properties.in(tag::any_handle) == tag::handles && !is_queue;
    static constexpr bool is_sequence = std::is_void_v<K> && !std::is_void_v<V> && !is_queue && !is_pool;
    /// Whether its elements can be visited in turn: all but a queue, which is reached by its front or top. A filter,
    /// which only answers membership, would not be either.
    static constexpr bool iterable = is_set || is_map || is_sequence;
    /// Whether it knows how many elements it holds: all but a pool with structural_sharing, which shares parts of its
    /// rows and keeps no list of them. A filter would not either. Every other pool counts its rows in O(1), and its
    /// handles are 0..size()-1: a row inserted new gets handle size().
    static constexpr bool sized = is_set || is_map || is_sequence || is_queue || is_partition || (is_pool && !urd_properties.has(tag::structural_sharing));

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

/// The kind a request is, from whether it has a key, a value, and the tags that make a queue or a pool.
consteval kind kind_of(const property_set& stated, bool keyed, bool valued)
{
    const bool queue = !keyed && valued && stated.in(tag::any_access) != tag::any_access;
    const bool pool = !keyed && valued && stated.has(tag::handles) && !queue;
    return { .set = keyed && !valued && !stated.has(tag::partition), .map = keyed && valued, .sequence = !keyed && valued && !queue && !pool, .queue = queue };
}

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
    // Positions are 0..n-1; a partition's classes are over indices.
    if (of.sequence || stated.has(tag::partition))
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
template<typename K, typename V, typename Order, listed_tags Listed, std::size_t... I>
spec<K, V, Order, Listed.tags[I]...> spec_of(std::index_sequence<I...> /*positions*/);

/// Checks a property list and turns it into its spec. Each static_assert names what is wrong.
template<typename... Ps>
struct to_spec
{
    static_assert(!(is_internal<Ps>() || ...),
                  "urd::Store: functional, immutable, growable and shrinkable are derived, never stated: a map has one value per key "
                  "unless it says multi, and frozen, monotone, nonempty and fixed_shape say how a Store may change");
    static_assert(((is_property<Ps> || is_internal<Ps>()) && ...),
                  "urd::Store: every argument must be an urd property (key<K>, value<V>, sorted, unique, …, or a "
                  "workload such as small)");
    static_assert((0 + ... + static_cast<int>(is_a<key, Ps>)) <= 1, "urd::Store: at most one key<K>");
    static_assert((0 + ... + static_cast<int>(is_a<value, Ps>)) <= 1, "urd::Store: at most one value<V>");
    static_assert(one_per_category({ tag_of<Ps>()... }),
                  "urd::Store: two properties answer the same question (dense and sparse, frozen and monotone, two "
                  "workloads, …)");

    using key_type = typename first_of<key, Ps...>::type;
    using value_type = typename first_of<value, Ps...>::type;
    static_assert(!std::is_void_v<key_type> || !std::is_void_v<value_type>, "urd::Store: needs key<K> (a set or map) or value<V> (a sequence)");

    static constexpr property_set stated = stated_with_access(stated_of({ tag_of<Ps>()... }));
    static constexpr bool keyed = !std::is_void_v<key_type>;
    static constexpr bool valued = !std::is_void_v<value_type>;
    static constexpr bool queue = !keyed && valued && stated.in(tag::any_access) != tag::any_access;
    static constexpr bool pool = !keyed && valued && stated.has(tag::handles) && !queue;
    static constexpr kind of = kind_of(stated, keyed, valued);

    static_assert(!stated.has(tag::partition) || (!valued && !stated.has(tag::frozen)),
                  "urd::Store: a partition groups key<K>s, without values, and changes by insert and merge");
    static_assert(!stated.has(tag::handles) || (!keyed && (!queue || stated.has(tag::by_priority)) && !stated.has(tag::positional) && !stated.has(tag::frozen)),
                  "urd::Store: a pool (handles) holds value<V> rows, gains them by insert, and is reached by handle, not by key, "
                  "position or order; a priority queue with handles changes or erases an element by its handle");
    static_assert(!stated.has(tag::flat) || (pool && stated.has(tag::unique)),
                  "urd::Store: flat stores each record of an interning pool as one block of bytes; ask for handles and unique too");
    static_assert(!(pool && stated.has(tag::unique)) || stated.has(tag::flat) || iterates<value_type>,
                  "urd::Store: a pool of records (value<V>, V no range of elements) stores them flat: ask for flat");
    static_assert(!stated.has(tag::monotone_priority) || stated.has(tag::by_priority),
                  "urd::Store: monotone_priority says how a priority queue is pushed to, so it makes a priority queue; it cannot go "
                  "with fifo or lifo");
    static_assert(!queue || (!stated.has(tag::frozen) && !stated.has(tag::positional)),
                  "urd::Store: a queue changes by push and pop, and is reached by its front or top, not by position");
    static_assert(!stated.has(tag::nonempty) || of.set || of.map || of.sequence,
                  "urd::Store: nonempty is a promise about the elements a set, map or sequence iterates; a queue empties by pop and a "
                  "pool by erase(handle), so it cannot hold there: drop nonempty, or check empty() before pop");
    static_assert(!stated.has(tag::fixed_shape) || (of.sequence && !stated.has(tag::monotone) && is_store<value_type>),
                  "urd::Store: fixed_shape is a sequence of rows (value<Store<…>>) whose number and lengths never change; its rows' "
                  "contents do");
    static_assert(!of.map || !stated.has(tag::positional), "urd::Store: positional identifies a map's elements by position, so it cannot have key<K>");
    static_assert(!(stated.has(tag::contiguous) && stated.has(tag::stable_refs)) || stated.has(tag::frozen),
                  "urd::Store: contiguous storage moves its elements when it grows, so stable_refs needs frozen too");
    static_assert(!((of.sequence || stated.has(tag::positional)) && stated.has(tag::stable_refs)) || stated.has(tag::monotone),
                  "urd::Store: erasing or inserting at a position moves the elements after it: stable_refs on a sequence or a "
                  "positional set needs monotone, append only, or frozen");
    static_assert(!(stated.has(tag::sorted) && stated.has(tag::concurrent)) || stated.has(tag::dense),
                  "urd::Store: no concurrent sorted container: threads share a hashed (concurrent) or dense Store; keep sorted and "
                  "synchronize outside, or drop sorted");
    static_assert(!queue || !stated.has(tag::concurrent),
                  "urd::Store: a queue is reached by its front or top, which pop moves: no queue is concurrent; guard it with a mutex, "
                  "or share a positional, monotone set");
    static_assert(!(stated.has(tag::frozen) && stated.has(tag::concurrent)),
                  "urd::Store: a frozen Store never changes, so any number of threads may read it; concurrent asks for inserts "
                  "from threads at once, which it has none of: drop concurrent");

    /// What order_by orders: keys, or a sequence's or queue's values.
    using ordered_type = std::conditional_t<keyed, key_type, value_type>;
    static constexpr bool ordered_by = (is_order_by<Ps> || ...);
    static_assert((0 + ... + static_cast<int>(is_order_by<Ps>)) <= 1, "urd::Store: at most one order_by<P>");
    static_assert(!ordered_by || stated.has(tag::sorted) || stated.has(tag::by_priority),
                  "urd::Store: order_by<P> orders a sorted Store or a by_priority queue; ask for one of them too");
    static_assert(!ordered_by || !keyed || stated.has(tag::multi) || orderable<ordered_type>,
                  "urd::Store: order_by<P> on unique keys breaks ties by the keys' own order, so they must be orderable "
                  "(else two keys with equal P(key) would be one)");
    using order_type = typename order_of<ordered_type, key_type, value_type, Ps...>::type;
    static_assert(!ordered_by || !stated.has(tag::dense),
                  "urd::Store: dense keys are stored and iterated by index, so no order_by<P> applies: drop dense, or order the index "
                  "type itself");
    static_assert(!ordered_by || !is_entry_less<order_type> || stated.has(tag::small) || stated.has(tag::frozen),
                  "urd::Store: a map ordered by its values is a flat scan (O(n) insert, lookup by key): ask for small or frozen too, or "
                  "order by key");
    static_assert(!(stated.has(tag::dense) && stated.has(tag::reclaims)),
                  "urd::Store: dense keys are stored by position (a slot per index), so erase returns no storage: reclaims needs sparse "
                  "keys (a tree or node table)");
    static_assert(!keyed || !stated.has(tag::constant_clear) || stated.has(tag::dense),
                  "urd::Store: constant_clear is a generation counter over a slot per index: ask for dense too (a hash table or tree "
                  "clears in time proportional to its slots: proportional_clear)");
    static_assert(!(stated.has(tag::layered) && stated.has(tag::sorted)),
                  "urd::Store: a layer iterates its own elements, not its parent's, so no order holds over both: drop sorted, or iterate "
                  "the parent separately");
    static_assert(!(stated.has(tag::partition) && stated.has(tag::concurrent)),
                  "urd::Store: a partition's merge rewrites parents along two paths: single-threaded");

    static constexpr listed_tags listed = non_roots(implied(stated, of));
    using type = decltype(detail::spec_of<key_type, value_type, order_type, listed>(std::make_index_sequence<listed.count> {}));

    /// The tags the request states (the roots left out), to probe one at a time where no representation serves it.
    static constexpr listed_tags stated_tags = non_roots(stated);
    /// What the request implies without `Reset` (its category back at its root, the rules applied again, so that
    /// without frozen the implied stable_refs goes too).
    template<tag Reset>
    static constexpr listed_tags listed_without =
        non_roots(implied(property_set(stated).put(root_of(Reset)), kind_of(property_set(stated).put(root_of(Reset)), keyed, valued)));
    /// The spec of the request without `Reset`: what it would resolve to then. It runs none of the checks above; it is
    /// for finding which tag an unservable request fails by.
    template<tag Reset>
    using without =
        decltype(detail::spec_of<key_type, value_type, order_type, listed_without<Reset>>(std::make_index_sequence<listed_without<Reset>.count> {}));
};

}  // namespace detail

}  // namespace urd
