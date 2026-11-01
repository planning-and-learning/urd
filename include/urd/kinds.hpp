#pragma once

// The kinds of Store, the top level of the registry tree: each a node that says when a request is of its kind
// (claims), what a request of its kind means beyond what it states (meaning), and how it keeps its elements. A kind
// added outside urd is a node like these, listed by specializing urd::extra_kinds; its containers are listed by
// specializing urd::detail::declarations_of (urd/registry/declarations.hpp). Nothing else names the kinds.

#include "urd/property_set.hpp"
#include "urd/registry/facts.hpp"
#include "urd/tags.hpp"

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace urd
{

using detail::property_set;

namespace detail
{

/// A list of facets (urd/store/): what a kind's Store offers, each where its condition holds.
template<template<typename, typename, typename> class... Facets>
struct facet_list
{
};

/// The roles a kind's containers rely on (urd/registry/facts.hpp, urd::role).
template<typename... Roles>
struct role_list
{
};

/// The categories a kind's requests answer (urd/tags.hpp): its request format.
template<category_enum... Categories>
struct request_format
{
};

/// The facets urd's kinds offer, defined in urd/store/<facet>.hpp.
namespace facet
{
template<typename Derived, typename R, typename S>
class sizing;
template<typename Derived, typename R, typename S>
class iteration;
template<typename Derived, typename R, typename S>
class equality;
template<typename Derived, typename R, typename S>
class ordering;
template<typename Derived, typename R, typename S>
class lookup;
template<typename Derived, typename R, typename S>
class ordered_lookup;
template<typename Derived, typename R, typename S>
class reverse_iteration;
template<typename Derived, typename R, typename S>
class positions;
template<typename Derived, typename R, typename S>
class insertion_positions;
template<typename Derived, typename R, typename S>
class contiguous_data;
template<typename Derived, typename R, typename S>
class capacity;
template<typename Derived, typename R, typename S>
class growth;
template<typename Derived, typename R, typename S>
class removal;
template<typename Derived, typename R, typename S>
class values_by_key;
template<typename Derived, typename R, typename S>
class multi_values;
template<typename Derived, typename R, typename S>
class set_algebra;
template<typename Derived, typename R, typename S>
class queue_access;
template<typename Derived, typename R, typename S>
class scopes;
template<typename Derived, typename R, typename S>
class layers;
template<typename Derived, typename R, typename S>
class changes;
template<typename Derived, typename R, typename S>
class pool;
template<typename Derived, typename R, typename S>
class records;
template<typename Derived, typename R, typename S>
class slots;
template<typename Derived, typename R, typename S>
class grouping;
template<typename Derived, typename R, typename S>
class entries;
template<typename Derived, typename R, typename S>
class inverse;
template<typename Derived, typename R, typename S>
class shape;
template<typename Derived, typename R, typename S>
class sorted_ends;
}  // namespace facet

/// The facets of every kind: each where its condition holds.
template<template<typename, typename, typename> class... Own>
using with_common_facets = facet_list<facet::sizing, facet::equality, facet::ordering, facet::ordered_lookup, facet::contiguous_data, facet::capacity, facet::scopes, facet::layers, facet::changes, facet::shape, Own...>;

}  // namespace detail

// What every kind shares: rules over the tags a request states or a container declares.

/// The tag of a container's threading and of its references, by a flag of its own: concurrent or not, stable or not.
constexpr tag threads(bool concurrent_) { return concurrent_ ? threading::concurrent : threading::none; }
constexpr tag references(bool stable) { return stable ? referencing::stable_refs : referencing::none; }

/// A preference: where a request holds `when`, containers holding `later` come after the others that serve it.
/// Contiguous storage is more compact and faster to search and iterate where elements are few or never change, so
/// stable storage (a node per element, segments that never move) comes last for them (docs/BENCHMARKS.md).
struct preference
{
    tag when = tag {};
    tag later = tag {};
};
inline constexpr std::array preferences { preference { .when = change::frozen, .later = referencing::stable_refs },
                                          preference { .when = workload::small, .later = referencing::stable_refs } };

/// What a Store held as an element of another gets beyond its own request: its storage given back when it is
/// destroyed, as erase and clear do.
inline constexpr std::array<tag, 1> nested_gives { memory::reclaims };

/// What a request asks of its container: what it means, but for what the Store keeps itself: once nothing changes
/// (frozen), nothing moves, so stable references need nothing of the container.
template<typename Set>
constexpr Set asked_of_container(Set stated)
{
    if (stated.has(change::frozen))
        stated.put(referencing::none);
    return stated;
}

/// Whether a request means something, whatever container would hold it: a partition groups keys, without values; a
/// pool (handles) is reached by handle, not by key; monotone_priority is a promise of a priority queue; order_by<P>
/// orders a sorted Store or a priority queue, and on unique keys breaks ties by the keys' own order, so they must be
/// orderable. Anything else no container serves fails in the registry.
template<typename Set>
constexpr bool well_formed(const Set& stated, bool keyed, bool valued, bool ordered_by, bool orderable)
{
    return (!stated.has(grouping::partition) || !valued) && (!stated.has(handling::handles) || !keyed)
           && (!stated.has(priority::monotone_priority) || stated.has(access::by_priority))
           && (!ordered_by || stated.has(order::sorted) || stated.has(access::by_priority))
           && (!ordered_by || !keyed || stated.has(multiplicity::multi) || orderable);
}

/// What a request promises its container: what it means, and that a Store which must stay nonempty never erases, so
/// a container that only grows serves it.
template<typename Set>
constexpr Set promised_to_container(Set meant)
{
    if (meant.has(emptiness::nonempty) && !meant.has(change::monotone))
        meant.put(change::monotone);
    return meant;
}

/// Keys in disjoint classes, reached by merge and find.
struct partition_kind
{
    static constexpr std::string_view name = "partition";
    /// The roles its containers rely on, beside what a request states.
    using roles = detail::role_list<role::element, role::order>;
    /// The questions its requests answer (its request format), in the order a spec lists its answers.
    using format = detail::request_format<order, uniqueness, keys, referencing, layout, representation, threading, memory, clearing, scoping, layering, sharing, grouping, tracking, change, emptiness, position, workload>;
    /// What its elements are, and what a key maps to (void: no mapped values), from key<K> and value<V>.
    template<typename K, typename V>
    using element_type = K;
    template<typename K, typename V>
    using mapped_type = void;
    static constexpr bool iterable = false;
    using facets = detail::with_common_facets<detail::facet::positions, detail::facet::grouping>;
    static constexpr bool keeps_by_value = false;
    static constexpr bool positional = false;
    static constexpr bool claims(const property_set& stated, bool /*keyed*/, bool /*valued*/) { return stated.has(grouping::partition); }
    static constexpr property_set meaning(property_set stated) { return stated; }
};

namespace detail
{

/// What both kinds of queue keep alike: values reached by their front or top, never iterated in an order.
struct queue_leaf
{
    /// The roles its containers rely on, beside what a request states.
    using roles = role_list<role::element, role::order>;
    /// What its elements are, and what a key maps to (void: no mapped values), from key<K> and value<V>.
    template<typename K, typename V>
    using element_type = V;
    template<typename K, typename V>
    using mapped_type = void;
    static constexpr bool iterable = false;
    using facets = with_common_facets<facet::iteration, facet::positions, facet::queue_access>;
    static constexpr bool keeps_by_value = false;
    static constexpr bool positional = false;
    static constexpr property_set meaning(property_set stated) { return stated; }
};

}  // namespace detail

/// Values reached by the order they arrived in: first in first out (fifo), or last in first out (lifo).
struct arrival_queue_kind : detail::queue_leaf
{
    static constexpr std::string_view name = "queue/arrival";
    /// The questions its requests answer (its request format), in the order a spec lists its answers.
    using format = detail::request_format<uniqueness, referencing, layout, representation, threading, memory, clearing, access, scoping, layering, handling, sharing, tracking, change, emptiness, position, workload>;
    static constexpr bool claims(const property_set& stated, bool /*keyed*/, bool /*valued*/)
    { return stated.has(access::fifo) || stated.has(access::lifo); }
};

/// Values reached by their priority: the top is the greatest by the order (order_by<P>, else the values' own).
struct priority_queue_kind : detail::queue_leaf
{
    static constexpr std::string_view name = "queue/priority";
    /// The questions its requests answer (its request format), in the order a spec lists its answers.
    using format = detail::request_format<order, uniqueness, referencing, layout, representation, threading, memory, clearing, access, scoping, layering, handling, sharing, tracking, change, emptiness, position, workload, priority>;
    static constexpr bool claims(const property_set& stated, bool /*keyed*/, bool /*valued*/)
    { return stated.has(access::by_priority) || stated.has(priority::monotone_priority); }
};

/// Values reached by their front or top: by arrival or by priority.
struct queue_kind
{
    static constexpr std::string_view name = "queue";
    static constexpr bool claims(const property_set& stated, bool keyed, bool valued)
    { return arrival_queue_kind::claims(stated, keyed, valued) || priority_queue_kind::claims(stated, keyed, valued); }
    using children = detail::type_list<priority_queue_kind, arrival_queue_kind>;
};

/// Values interned by handle, each distinct one once: rows (a range each, by row) or records and values (one each,
/// by index; records copied as bytes can be stored flat).
struct pool_kind
{
    static constexpr std::string_view name = "pool";
    /// The roles its containers rely on, beside what a request states.
    using roles = detail::role_list<role::element, role::row, role::row_element>;
    /// The questions its requests answer (its request format), in the order a spec lists its answers.
    using format = detail::request_format<order, uniqueness, keys, referencing, layout, representation, threading, memory, clearing, scoping, layering, handling, sharing, packing, tracking, change, emptiness, position, workload>;
    /// What its elements are, and what a key maps to (void: no mapped values), from key<K> and value<V>.
    template<typename K, typename V>
    using element_type = V;
    template<typename K, typename V>
    using mapped_type = void;
    static constexpr bool iterable = false;
    using facets = detail::with_common_facets<detail::facet::positions, detail::facet::records, detail::facet::pool>;
    static constexpr bool keeps_by_value = false;
    static constexpr bool positional = false;
    static constexpr bool claims(const property_set& stated, bool /*keyed*/, bool /*valued*/)
    { return stated.has(handling::handles) && stated.has(uniqueness::unique); }
    static constexpr property_set meaning(property_set stated) { return stated; }
};

/// Values in slots, by handle.
struct slots_kind
{
    static constexpr std::string_view name = "slots";
    /// The roles its containers rely on, beside what a request states.
    using roles = detail::role_list<role::element, role::order>;
    /// The questions its requests answer (its request format), in the order a spec lists its answers.
    using format = detail::request_format<order, uniqueness, keys, referencing, layout, representation, threading, memory, clearing, scoping, layering, handling, sharing, tracking, change, emptiness, position, workload>;
    /// What its elements are, and what a key maps to (void: no mapped values), from key<K> and value<V>.
    template<typename K, typename V>
    using element_type = V;
    template<typename K, typename V>
    using mapped_type = void;
    static constexpr bool iterable = false;
    using facets = detail::with_common_facets<detail::facet::positions, detail::facet::slots>;
    static constexpr bool keeps_by_value = false;
    static constexpr bool positional = false;
    static constexpr bool claims(const property_set& stated, bool /*keyed*/, bool /*valued*/) { return stated.has(handling::handles); }
    static constexpr property_set meaning(property_set stated) { return stated; }
};

/// A value per key, or several (multi): without multi, each key once.
struct map_kind
{
    static constexpr std::string_view name = "map";
    /// The roles its containers rely on, beside what a request states.
    using roles = detail::role_list<role::element, role::order, role::mapped>;
    /// The questions its requests answer (its request format), in the order a spec lists its answers.
    using format = detail::request_format<order, uniqueness, keys, referencing, layout, representation, threading, memory, clearing, scoping, layering, sharing, direction, tracking, change, emptiness, multiplicity, position, workload>;
    /// What its elements are, and what a key maps to (void: no mapped values), from key<K> and value<V>.
    template<typename K, typename V>
    using element_type = std::pair<const K, V>;
    template<typename K, typename V>
    using mapped_type = V;
    static constexpr bool iterable = true;
    using facets = detail::with_common_facets<detail::facet::iteration, detail::facet::lookup, detail::facet::reverse_iteration, detail::facet::positions, detail::facet::growth, detail::facet::removal, detail::facet::values_by_key, detail::facet::multi_values, detail::facet::entries, detail::facet::inverse, detail::facet::sorted_ends>;
    static constexpr bool keeps_by_value = true;
    static constexpr bool positional = false;
    static constexpr bool claims(const property_set& /*stated*/, bool keyed, bool valued) { return keyed && valued; }
    static constexpr property_set meaning(property_set stated)
    {
        if (!stated.has(multiplicity::multi))
            stated.put(uniqueness::unique);
        return stated;
    }
};

/// Keys, each once; with positions it numbers its keys as they come (an interner), unless it asks for another order.
struct set_kind
{
    static constexpr std::string_view name = "set";
    /// The roles its containers rely on, beside what a request states.
    using roles = detail::role_list<role::element, role::order>;
    /// The questions its requests answer (its request format), in the order a spec lists its answers.
    using format = detail::request_format<order, uniqueness, keys, referencing, layout, representation, threading, memory, clearing, scoping, layering, sharing, tracking, change, emptiness, position, workload>;
    /// What its elements are, and what a key maps to (void: no mapped values), from key<K> and value<V>.
    template<typename K, typename V>
    using element_type = K;
    template<typename K, typename V>
    using mapped_type = void;
    static constexpr bool iterable = true;
    using facets = detail::with_common_facets<detail::facet::iteration, detail::facet::lookup, detail::facet::reverse_iteration, detail::facet::insertion_positions, detail::facet::growth, detail::facet::removal, detail::facet::set_algebra, detail::facet::sorted_ends>;
    static constexpr bool keeps_by_value = true;
    static constexpr bool positional = false;
    static constexpr bool claims(const property_set& /*stated*/, bool keyed, bool /*valued*/) { return keyed; }
    static constexpr property_set meaning(property_set stated)
    {
        stated.put(uniqueness::unique);
        if (stated.has(position::positional) && stated.get<order>() == order::none)
            stated.put(order::insertion_ordered);
        return stated;
    }
};

/// Values kept by value, in order, duplicates kept unless unique.
struct multiset_kind
{
    static constexpr std::string_view name = "multiset";
    /// The roles its containers rely on, beside what a request states.
    using roles = detail::role_list<role::element, role::order>;
    /// The questions its requests answer (its request format), in the order a spec lists its answers.
    using format = detail::request_format<order, uniqueness, keys, referencing, layout, representation, threading, memory, clearing, scoping, layering, sharing, tracking, change, emptiness, multiplicity, position, workload>;
    /// What its elements are, and what a key maps to (void: no mapped values), from key<K> and value<V>.
    template<typename K, typename V>
    using element_type = V;
    template<typename K, typename V>
    using mapped_type = void;
    static constexpr bool iterable = true;
    using facets = detail::with_common_facets<detail::facet::iteration, detail::facet::lookup, detail::facet::reverse_iteration, detail::facet::positions, detail::facet::growth, detail::facet::removal>;
    static constexpr bool keeps_by_value = true;
    static constexpr bool positional = true;
    static constexpr bool claims(const property_set& stated, bool /*keyed*/, bool /*valued*/)
    { return stated.has(order::sorted) || stated.has(uniqueness::unique) || stated.has(multiplicity::multi); }
    static constexpr property_set meaning(property_set stated) { return stated; }
};

/// Values by position.
struct sequence_kind
{
    static constexpr std::string_view name = "sequence";
    /// The roles its containers rely on, beside what a request states.
    using roles = detail::role_list<role::element, role::order, role::rows>;
    /// The questions its requests answer (its request format), in the order a spec lists its answers.
    using format = detail::request_format<order, keys, referencing, layout, representation, threading, memory, clearing, scoping, layering, sharing, shape, tracking, change, emptiness, position, workload>;
    /// What its elements are, and what a key maps to (void: no mapped values), from key<K> and value<V>.
    template<typename K, typename V>
    using element_type = V;
    template<typename K, typename V>
    using mapped_type = void;
    static constexpr bool iterable = true;
    using facets = detail::with_common_facets<detail::facet::iteration, detail::facet::lookup, detail::facet::reverse_iteration, detail::facet::positions, detail::facet::growth, detail::facet::removal>;
    static constexpr bool keeps_by_value = false;
    static constexpr bool positional = true;
    static constexpr bool claims(const property_set& /*stated*/, bool /*keyed*/, bool /*valued*/) { return true; }
    static constexpr property_set meaning(property_set stated) { return stated; }
};

/// Kinds added outside urd, tried before urd's own: specialize extra_kinds<void> with `using type =
/// detail::type_list<my_kind, …>;` before the first Store is named.
template<typename = void>
struct extra_kinds
{
    using type = detail::type_list<>;
};

/// The kinds at the top of the tree, in the order they claim a request: the first that claims it is its kind (a
/// priority queue with handles is a queue). The last claims every request.
/// A template, so that kinds added outside urd are read where a request is resolved, not here.
template<typename Unused = void>
using kinds = typename detail::join<typename extra_kinds<Unused>::type, detail::type_list<partition_kind, queue_kind, pool_kind, slots_kind, map_kind, set_kind, multiset_kind, sequence_kind>>::type;

namespace detail
{

template<typename... Kinds>
constexpr std::size_t claiming(type_list<Kinds...> /*kinds*/, property_set stated, bool keyed, bool valued)
{
    std::size_t at = 0;
    const bool found = ((Kinds::claims(stated, keyed, valued) || (++at, false)) || ...);
    return found ? at : sizeof...(Kinds);
}
template<std::size_t I, typename List>
struct kind_at;
template<std::size_t I, typename... Kinds>
struct kind_at<I, type_list<Kinds...>>
{
    using type = std::tuple_element_t<I, std::tuple<Kinds...>>;
};

/// The node claiming a request among List, and below it, the child claiming it, down to a leaf. Structs and
/// std::conditional_t, not aliases over the call or partial specializations over the request: GCC 13 fails on those
/// internally.
template<typename List, property_set Stated, bool Keyed, bool Valued>
struct claimed;
template<typename Node, property_set Stated, bool Keyed, bool Valued>
struct claimed_below
{
    using type = typename claimed<typename Node::children, Stated, Keyed, Valued>::type;
};
template<typename List, property_set Stated, bool Keyed, bool Valued>
struct claimed
{
    static constexpr std::size_t at = claiming(List {}, Stated, Keyed, Valued);
    using node = typename kind_at<at, List>::type;
    using type = typename std::conditional_t<requires { typename node::children; }, claimed_below<node, Stated, Keyed, Valued>, std::type_identity<node>>::type;
};

/// The leaves below a list of nodes, in order.
template<typename List>
struct leaves_of;
template<typename Node>
struct leaves_below
{
    using type = type_list<Node>;
};
template<typename Node>
    requires requires { typename Node::children; }
struct leaves_below<Node>
{
    using type = typename leaves_of<typename Node::children>::type;
};
template<typename... Nodes>
struct leaves_of<type_list<Nodes...>>
{
    using type = typename join<typename leaves_below<Nodes>::type...>::type;
};

/// The kinds at the top, named where a request is resolved (dependent on it, so not before).
template<bool Any>
struct root_kinds
{
    using type = kinds<std::conditional_t<Any, void, void>>;  // void, but dependent: read when a request is resolved
};

/// Whether Node is Parent or below it.
template<typename Node, typename Parent>
inline constexpr bool below = std::is_same_v<Node, Parent>;
template<typename Node, typename Parent>
    requires requires { typename Parent::children; }
inline constexpr bool below<Node, Parent> = std::is_same_v<Node, Parent> || []<typename... Children>(type_list<Children...> /*children*/)
{ return (below<Node, Children> || ...); }(typename Parent::children {});

}  // namespace detail

/// The kind of a request: the first kind that claims it, from the tags it states and its shape (key<K>, value<V>),
/// and below it the first child that claims it, down to a leaf: a kind with containers, a format and facets.
template<property_set Stated, bool Keyed, bool Valued>
using kind_of = typename detail::claimed<typename detail::root_kinds<Keyed>::type, Stated, Keyed, Valued>::type;

/// The leaves of the tree of kinds: what a Store's kind can be.
template<typename Unused = void>
using leaf_kinds = typename detail::leaves_of<kinds<Unused>>::type;

/// Whether Kind is Node or a kind below it (a priority queue is a queue).
template<typename Kind, typename Node>
concept kind_below = detail::below<Kind, Node>;

/// What the tags a request states imply, whatever its kind: once nothing changes (frozen), nothing moves.
constexpr property_set implied(property_set stated)
{
    if (stated.has(change::frozen))
        stated.put(referencing::stable_refs);
    return stated;
}

/// What a request of kind Kind means beyond what it states: what its kind keeps (a set keeps each key once; a map
/// without multi too), and what its tags imply.
template<typename Kind>
constexpr property_set meaning(property_set stated)
{ return implied(Kind::meaning(stated)); }

}  // namespace urd
