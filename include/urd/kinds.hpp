#pragma once

// The kinds of Store. A kind is a struct: its name, the questions its requests answer (format), its Store's interface
// (facets), what a request of it means, and its rules. It derives from detail::any_kind, which says what every Store
// is, and states only what differs; generic code reads the members and makes no choice. A request names its kind as a
// template over its element types (set<K>, map<K, V>, sequence<V>, …): the kind with its data. A kind added outside urd
// is a struct and a template like these; its containers are added by specializing urd::detail::declarations_of
// (urd/registry/declarations.hpp).

#include "urd/detail/type_list.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/property_set.hpp"
#include "urd/registry/facts.hpp"
#include "urd/tags.hpp"

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace urd
{

using detail::property_set;

/// A preference: where a request holds `when`, containers holding `later` come after the others that serve it.
struct preference
{
    tag when = tag {};
    tag later = tag {};
};

namespace detail
{

/// A list of facets (urd/store/): what a kind's Store offers, each where its condition holds.
template<template<typename, typename, typename> class... Facets>
struct facet_list
{
};
/// Whether F is among the facets of a list.
template<template<typename, typename, typename> class F, typename List>
inline constexpr bool listed_facet = false;
template<template<typename, typename, typename> class F, template<typename, typename, typename> class... Fs>
inline constexpr bool listed_facet<F, facet_list<Fs...>> = (std::is_same_v<facet_list<F>, facet_list<Fs>> || ...);
template<template<typename, typename, typename> class... As, template<typename, typename, typename> class... Bs, typename... Rest>
struct concat<facet_list<As...>, facet_list<Bs...>, Rest...> : concat<facet_list<As..., Bs...>, Rest...>
{
};

/// The categories a kind's requests answer (urd/tags.hpp): its request format.
template<typename... Categories>
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
class visiting;
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

/// Which type plays which role in a request's data.
template<typename Role, typename T>
struct plays
{
    using role = Role;
    using type = T;
};
template<typename... Plays>
struct role_types
{
};

/// What every Store is, and what a kind is unless it says otherwise. A kind derives from it and restates what
/// differs: its own questions (asked before the common ones), its own facets (after the common ones), what a request
/// means, its rules.
struct any_kind
{
    /// The questions every kind's requests answer: how it stores, what it costs, how it is used.
    using common_format = request_format<referencing,
                                         layout,
                                         representation,
                                         threading,
                                         memory,
                                         clearing,
                                         scoping,
                                         layering,
                                         sharing,
                                         tracking,
                                         change,
                                         emptiness,
                                         position,
                                         comparison,
                                         workload>;
    /// The facets every kind's Store has, each where its condition holds.
    using common_facets = facet_list<facet::sizing,
                                     facet::equality,
                                     facet::ordering,
                                     facet::ordered_lookup,
                                     facet::contiguous_data,
                                     facet::capacity,
                                     facet::scopes,
                                     facet::layers,
                                     facet::changes,
                                     facet::shape>;

    using format = request_format<>;
    using facets = facet_list<>;

    /// What a request means beyond what it states: once nothing changes (frozen), nothing moves.
    static constexpr property_set meaning(property_set stated)
    {
        if (stated.has(change::frozen))
            stated.put(referencing::stable_refs);
        return stated;
    }
    /// What it asks of its container: what it means, but for what the Store keeps itself: once nothing changes
    /// (frozen), nothing moves, so stable references need nothing of the container.
    static constexpr property_set asks(property_set meant)
    {
        if (meant.has(change::frozen))
            meant.put(referencing::none);
        return meant;
    }
    /// What it promises its container: what it means, and that a Store which must stay nonempty never erases, so a
    /// container that only grows serves it.
    static constexpr property_set promises(property_set meant)
    {
        if (meant.has(emptiness::nonempty) && !meant.has(change::monotone))
            meant.put(change::monotone);
        return meant;
    }
    /// Which containers come after the others that serve it. Contiguous storage is more compact and faster to search
    /// and iterate where elements are few or never change, so stable storage (a node per element, segments that never
    /// move) comes last for them (docs/BENCHMARKS.md).
    static constexpr std::array prefers { preference { .when = change::frozen, .later = referencing::stable_refs },
                                          preference { .when = workload::small, .later = referencing::stable_refs } };
    static constexpr bool well_formed(const property_set& /*stated*/) { return true; }
    static constexpr bool accepts_order_by(const property_set& stated) { return stated.has(order::sorted); }
    static constexpr bool changes_in_place(const property_set& /*row*/) { return true; }
    /// What a Store of it is built from besides its elements (spec S): nothing.
    template<typename S, typename From>
    static constexpr bool builds_from = false;
};

}  // namespace detail

/// Keys in disjoint classes, reached by merge and find.
struct partition_kind : detail::any_kind
{
    static constexpr std::string_view name = "partition";
    using format = detail::request_format<order, uniqueness, keys, grouping>;
    using facets = detail::facet_list<detail::facet::positions, detail::facet::grouping>;
    /// Its keys fall into classes.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(grouping::partition)); }
};

/// Values reached by their front or top: by arrival, first in first out (fifo) or last in first out (lifo), or by
/// priority (by_priority: the top is the greatest by the order, order_by<C>, else urd::Less). Visited in no order
/// urd reads.
struct queue_kind : detail::any_kind
{
    static constexpr std::string_view name = "queue";
    using format = detail::request_format<order, uniqueness, access, handling, priority>;
    using facets = detail::facet_list<detail::facet::visiting, detail::facet::positions, detail::facet::queue_access>;
    /// A queue is reached somehow (fifo, lifo or by_priority); monotone_priority is a promise of one reached by priority.
    static constexpr bool well_formed(const property_set& stated)
    { return stated.get<access>() != access::none && (!stated.has(priority::monotone_priority) || stated.has(access::by_priority)); }
    /// Its order, by priority.
    static constexpr bool accepts_order_by(const property_set& stated) { return stated.has(access::by_priority); }
};

/// Values interned by handle, each distinct one once: rows (a range each, by row) or records and values (one each,
/// by index; records copied as bytes can be stored flat).
struct pool_kind : detail::any_kind
{
    static constexpr std::string_view name = "pool";
    using format = detail::request_format<order, uniqueness, keys, handling, packing>;
    using facets = detail::facet_list<detail::facet::positions, detail::facet::records, detail::facet::pool>;
    /// Each distinct value once, by handle.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(handling::handles).put(uniqueness::unique)); }
};

/// Values in slots, by handle.
struct slots_kind : detail::any_kind
{
    static constexpr std::string_view name = "slots";
    using format = detail::request_format<order, uniqueness, keys, handling>;
    using facets = detail::facet_list<detail::facet::positions, detail::facet::slots>;
    /// Its values are reached by handle.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(handling::handles)); }
};

/// A value per key, or several (multi): without multi, each key once.
struct map_kind : detail::any_kind
{
    static constexpr std::string_view name = "map";
    using format = detail::request_format<order, uniqueness, keys, direction, multiplicity>;
    using facets = detail::facet_list<detail::facet::iteration,
                                      detail::facet::lookup,
                                      detail::facet::reverse_iteration,
                                      detail::facet::positions,
                                      detail::facet::growth,
                                      detail::facet::removal,
                                      detail::facet::values_by_key,
                                      detail::facet::multi_values,
                                      detail::facet::entries,
                                      detail::facet::inverse,
                                      detail::facet::sorted_ends>;
    /// Without multi, each key once.
    static constexpr property_set meaning(property_set stated)
    {
        if (!stated.has(multiplicity::multi))
            stated.put(uniqueness::unique);
        return any_kind::meaning(stated);
    }
    static constexpr bool changes_in_place(const property_set& /*row*/) { return false; }
};

/// Keys, each once; with positions it numbers its keys as they come (an interner), unless it asks for another order.
struct set_kind : detail::any_kind
{
    static constexpr std::string_view name = "set";
    using format = detail::request_format<order, uniqueness, keys>;
    using facets = detail::facet_list<detail::facet::iteration,
                                      detail::facet::lookup,
                                      detail::facet::reverse_iteration,
                                      detail::facet::insertion_positions,
                                      detail::facet::growth,
                                      detail::facet::removal,
                                      detail::facet::set_algebra,
                                      detail::facet::sorted_ends>;
    /// Each key once; with positions and no order, numbered as they come.
    static constexpr property_set meaning(property_set stated)
    {
        stated.put(uniqueness::unique);
        if (stated.has(position::positional) && stated.get<order>() == order::none)
            stated.put(order::insertion_ordered);
        return any_kind::meaning(stated);
    }
    /// A set of dense keys is bits, set and cleared in place.
    static constexpr bool changes_in_place(const property_set& row) { return row.has(uniqueness::unique) && row.has(keys::dense); }
};

/// Values kept by value, in order, duplicates kept unless unique.
struct multiset_kind : detail::any_kind
{
    static constexpr std::string_view name = "multiset";
    using format = detail::request_format<order, uniqueness, keys, multiplicity>;
    using facets = detail::facet_list<detail::facet::iteration,
                                      detail::facet::lookup,
                                      detail::facet::reverse_iteration,
                                      detail::facet::positions,
                                      detail::facet::growth,
                                      detail::facet::removal>;
    /// Its elements have positions.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(position::positional)); }
    static constexpr bool changes_in_place(const property_set& /*row*/) { return false; }
};

/// Values by position.
struct sequence_kind : detail::any_kind
{
    static constexpr std::string_view name = "sequence";
    using format = detail::request_format<order, keys, shape>;
    using facets = detail::facet_list<detail::facet::iteration,
                                      detail::facet::lookup,
                                      detail::facet::reverse_iteration,
                                      detail::facet::positions,
                                      detail::facet::growth,
                                      detail::facet::removal>;
    /// Its elements have positions.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(position::positional)); }
    /// Rows of a fixed shape are built from their lengths (urd::row_lengths), or for rows of rows, each row's shape.
    template<typename S, typename From>
    static constexpr bool builds_from = S::urd_properties.has(shape::fixed_shape);
};

/// urd's kinds: what its tables list.
using kinds = detail::type_list<set_kind, map_kind, multiset_kind, sequence_kind, queue_kind, pool_kind, slots_kind, partition_kind>;

namespace detail
{

/// A kind Kind with values of type V as its data (a sequence, multiset, queue, pool, slots): what a request names.
template<typename Kind, typename V>
struct values_of : Kind
{
    using kind = Kind;
    using element_types = type_list<V>;
    using value_type = V;
    using element_type = V;
    /// What may be a Store (a row).
    using held_type = V;
    /// What find takes, where the kind has lookup.
    using lookup_type = V;
    /// What its order compares.
    using ordered_type = V;
    using played = role_types<plays<role::element, V>>;
};

/// A kind Kind with keys of type K as its data (a set, a partition).
template<typename Kind, typename K>
struct keys_of : Kind
{
    using kind = Kind;
    using element_types = type_list<K>;
    using key_type = K;
    using element_type = K;
    using held_type = K;
    using lookup_type = K;
    using ordered_type = K;
    using played = role_types<plays<role::element, K>>;
};

}  // namespace detail

// What a request names first: its kind, with its element types.

/// Keys of type K, each once.
template<typename K>
struct set : detail::keys_of<set_kind, K>
{
};
/// A value of type V per key of type K.
template<typename K, typename V>
struct map : map_kind
{
    using kind = map_kind;
    using element_types = detail::type_list<K, V>;
    using key_type = K;
    using mapped_type = V;
    using element_type = std::pair<const K, V>;
    using held_type = V;
    using lookup_type = K;
    using ordered_type = K;
    using played = detail::role_types<detail::plays<role::element, K>, detail::plays<role::mapped, V>>;
};
/// Values of type V kept by value, in order.
template<typename V>
struct multiset : detail::values_of<multiset_kind, V>
{
};
/// Values of type V by position.
template<typename V>
struct sequence : detail::values_of<sequence_kind, V>
{
};
/// Values of type V reached by their front or top.
template<typename V>
struct queue : detail::values_of<queue_kind, V>
{
};
/// Values of type V interned by handle, each distinct one once.
template<typename V>
struct pool : detail::values_of<pool_kind, V>
{
};
/// Values of type V in slots, by handle.
template<typename V>
struct slots : detail::values_of<slots_kind, V>
{
};
/// Keys of type K in disjoint classes.
template<typename K>
struct partition : detail::keys_of<partition_kind, K>
{
};

namespace detail
{

/// The questions a request of Kind answers (its own, then every kind's), and its Store's facets (every kind's, then
/// its own).
template<typename Kind>
using format_of = typename concat<typename Kind::format, typename any_kind::common_format>::type;
template<typename Kind>
using facets_of = typename concat<typename any_kind::common_facets, typename Kind::facets>::type;

}  // namespace detail

/// Whether a request of kind Kind contradicts itself, whatever container would hold it: by its kind's rule, and
/// order_by<C> where its kind orders by nothing it asks for. Anything else no container serves fails in the registry.
template<typename Kind>
constexpr bool well_formed(const property_set& stated)
{ return Kind::well_formed(stated) && (!stated.has(comparison::given) || Kind::accepts_order_by(stated)); }

/// What a request of kind Kind means beyond what it states: what its kind keeps (a set keeps each key once; a map
/// without multi too; a sequence's elements have positions), and what its tags imply.
template<typename Kind>
constexpr property_set meaning(property_set stated)
{ return Kind::meaning(stated); }

}  // namespace urd
