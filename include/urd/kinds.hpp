#pragma once

// The kinds of Store. A kind is a struct: its name, the questions its requests answer (format), its Store's interface
// (facets), what a request of it means, and its rules. It derives from detail::any_kind, which says what every Store
// is, and states only what differs; generic code reads the members and makes no choice. A request names its kind as a
// template over its element types (set<K>, map<K, V>, sequence<V>, …): the kind with its data. A kind added outside urd
// is a struct and a template like these; its containers are added by specializing urd::detail::declarations_of
// (urd/registry/declarations.hpp).

#include <boost/mp11.hpp>
#include "urd/elements/index.hpp"
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
/// Two facet lists joined in order (facets are templates, so boost::mp11::mp_append does not apply).
template<typename A, typename B>
struct join_facets;
template<template<typename, typename, typename> class... As, template<typename, typename, typename> class... Bs>
struct join_facets<facet_list<As...>, facet_list<Bs...>>
{
    using type = facet_list<As..., Bs...>;
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
class positions;
template<typename Derived, typename R, typename S>
class insertion_positions;
template<typename Derived, typename R, typename S>
class contiguous_data;
template<typename Derived, typename R, typename S>
class reserving;
template<typename Derived, typename R, typename S>
class insertion;
template<typename Derived, typename R, typename S>
class appending;
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
class ends;
}  // namespace facet

/// Which types play role Role in a Store's data: its element type, or for the order the ordered type and its
/// comparator.
template<typename Role, typename... Args>
struct plays
{
    using role = Role;
};

/// What every Store is, and what a kind is unless it says otherwise. A kind derives from it and restates what
/// differs: its own questions (asked before the common ones), its own facets (after the common ones), what a request
/// means, its rules.
struct any_kind
{
    /// The questions every kind's requests answer: how it stores, what it costs, how it is used.
    using common_format = request_format<referencing,
                                         layout,
                                         width,
                                         representation,
                                         threading,
                                         memory,
                                         clearing,
                                         scoping,
                                         layering,
                                         sharing,
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
                                     facet::reserving,
                                     facet::scopes,
                                     facet::layers,
                                     facet::shape,
                                     facet::ends>;

    using format = request_format<>;
    using facets = facet_list<>;

    /// What a request means beyond what it states: what its tags imply (urd::tag_defaults, implies: frozen means
    /// stable_refs).
    static constexpr property_set meaning(property_set stated)
    {
        auto result = stated;
        stated.for_each_listed<&category_table::implies>([&result](tag implied) { result.put(implied); });
        return result;
    }
    /// What it asks of its container: what it means, but for what the Store keeps itself (kept_by_store: a frozen
    /// Store's references stay put whatever holds it).
    static constexpr property_set asks(property_set meant)
    {
        auto result = meant;
        meant.for_each_listed<&category_table::kept_by_store>([&result](tag kept) { result.put(tag(kept.in, 0, kept.role)); });
        return result;
    }
    /// What it promises its container: what it means, and what its tags promise where it holds less (promises: a
    /// Store that stays nonempty never erases, so it only grows).
    static constexpr property_set promises(property_set meant)
    {
        auto result = meant;
        meant.for_each_listed<&category_table::promises>(
            [&result](tag promised)
            {
                if (!result.has(promised))
                    result.put(promised);
            });
        return result;
    }
    /// The page of the docs (docs/kinds/<page>.rst) and tables that describes a Store of it, by what it states.
    static constexpr std::string_view page(const property_set& /*stated*/) { return {}; }
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
    static constexpr std::string_view page(const property_set& /*stated*/) { return "partitions"; }
    using format = detail::request_format<order, uniqueness, keys, grouping>;
    using facets = detail::facet_list<detail::facet::grouping>;
    /// Its keys fall into classes.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(grouping::partition)); }
};

/// Values reached by their front or top: by arrival, first in first out (fifo) or last in first out (lifo), or by
/// priority (by_priority: the top is the least by the order, order_by<C>, else urd::Less). Visited in no order
/// urd reads.
struct queue_kind : detail::any_kind
{
    static constexpr std::string_view name = "queue";
    static constexpr std::string_view page(const property_set& /*stated*/) { return "queues"; }
    using format = detail::request_format<order, uniqueness, access, handling, priority>;
    using facets = detail::facet_list<detail::facet::visiting, detail::facet::queue_access>;
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
    /// Records stored flat have a page of their own.
    static constexpr std::string_view page(const property_set& stated) { return stated.has(packing::flat) ? "records" : "pools"; }
    using format = detail::request_format<order, uniqueness, keys, handling, packing>;
    using facets = detail::facet_list<detail::facet::records, detail::facet::pool>;
    /// Each distinct value once, by handle.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(handling::handles).put(uniqueness::unique)); }
};

/// Values in slots, by handle.
struct slots_kind : detail::any_kind
{
    static constexpr std::string_view name = "slots";
    /// Records stored flat have a page of their own.
    static constexpr std::string_view page(const property_set& stated) { return stated.has(packing::flat) ? "records" : "pools"; }
    using format = detail::request_format<order, uniqueness, keys, handling>;
    using facets = detail::facet_list<detail::facet::slots>;
    /// Its values are reached by handle.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(handling::handles)); }
};

/// A value per key, or several (multi): without multi, each key once.
struct map_kind : detail::any_kind
{
    static constexpr std::string_view name = "map";
    static constexpr std::string_view page(const property_set& /*stated*/) { return "maps"; }
    using format = detail::request_format<order, uniqueness, keys, direction, multiplicity>;
    using facets = detail::facet_list<detail::facet::iteration,
                                      detail::facet::lookup,
                                      detail::facet::positions,
                                      detail::facet::insertion,
                                      detail::facet::removal,
                                      detail::facet::values_by_key,
                                      detail::facet::multi_values,
                                      detail::facet::entries,
                                      detail::facet::inverse>;
    /// Without multi, each key once.
    static constexpr property_set meaning(property_set stated)
    {
        if (!stated.has(multiplicity::multi))
            stated.put(uniqueness::unique);
        return any_kind::meaning(stated);
    }
    /// Positions are a multimap's: a map with each key once is reached by key (at, []), which positions would name
    /// again.
    static constexpr bool well_formed(const property_set& stated) { return !stated.has(position::positional) || stated.has(multiplicity::multi); }
    static constexpr bool changes_in_place(const property_set& /*row*/) { return false; }
};

/// Keys, each once; with positions it numbers its keys as they come (an interner), unless it asks for another order.
struct set_kind : detail::any_kind
{
    static constexpr std::string_view name = "set";
    static constexpr std::string_view page(const property_set& /*stated*/) { return "sets"; }
    using format = detail::request_format<order, uniqueness, keys>;
    using facets = detail::facet_list<detail::facet::iteration,
                                      detail::facet::lookup,
                                      detail::facet::insertion_positions,
                                      detail::facet::insertion,
                                      detail::facet::removal,
                                      detail::facet::set_algebra>;
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
    static constexpr std::string_view page(const property_set& /*stated*/) { return "sequences"; }
    using format = detail::request_format<order, uniqueness, keys, multiplicity>;
    using facets =
        detail::facet_list<detail::facet::iteration, detail::facet::lookup, detail::facet::positions, detail::facet::insertion, detail::facet::removal>;
    /// Its elements have positions.
    static constexpr property_set meaning(property_set stated) { return any_kind::meaning(stated.put(position::positional)); }
    static constexpr bool changes_in_place(const property_set& /*row*/) { return false; }
};

/// Values by position.
struct sequence_kind : detail::any_kind
{
    static constexpr std::string_view name = "sequence";
    static constexpr std::string_view page(const property_set& /*stated*/) { return "sequences"; }
    using format = detail::request_format<order, keys, shape>;
    using facets = detail::facet_list<detail::facet::iteration, detail::facet::positions, detail::facet::appending, detail::facet::removal>;
    /// Its elements have positions; appended from several threads, they never move (what another thread reads stays
    /// where it is).
    static constexpr property_set meaning(property_set stated)
    {
        stated = stated.put(position::positional);
        if (stated.has(threading::concurrent))
            stated = stated.put(referencing::stable_refs);
        return any_kind::meaning(stated);
    }
    /// Rows of a fixed shape are built from their lengths (urd::row_lengths), or for rows of rows, each row's shape.
    template<typename S, typename From>
    static constexpr bool builds_from = S::urd_properties.has(shape::fixed_shape);
};

/// urd's kinds: what its tables list.
using kinds = boost::mp11::mp_list<set_kind, map_kind, multiset_kind, sequence_kind, queue_kind, pool_kind, slots_kind, partition_kind>;

namespace detail
{

/// A kind Kind with its data: its elements, of type Element, and for a map its values, of type Mapped (else void).
/// What a request names derives from it (urd::set<K>, …) and adds its own names (key_type, value_type, mapped_type).
template<typename Kind, typename Element, typename Mapped = void>
struct data_of : Kind
{
    using kind = Kind;
    using element_types = boost::mp11::mp_list<Element>;
    using element_type = Element;
    /// What may be a Store (a row): a map's values, else its elements.
    using held_type = std::conditional_t<std::is_void_v<Mapped>, Element, Mapped>;
    /// What find takes, where the kind has lookup.
    using lookup_type = Element;
    /// What addresses its elements, where it has positions (operator[], at): a number.
    using position_type = std::size_t;
    /// What its order compares.
    using ordered_type = Element;
    /// Which of its types plays which role, ordered by Order: its elements are its elements and are ordered, a map's
    /// values are mapped (void elsewhere: no facts), what it holds are its rows, and its rows' elements (void where it
    /// holds no rows) are its row elements.
    template<typename Order>
    using played = boost::mp11::mp_list<plays<role::element, Element>,
                             plays<role::mapped, Mapped>,
                             plays<role::order, Element, Order>,
                             plays<role::row, held_type>,
                             plays<role::rows, held_type>,
                             plays<role::row_element, typename held_as<held_type>::elements>>;
};

}  // namespace detail

// What a request names first: its kind, with its element types.

/// Keys of type K, each once.
template<typename K>
struct set : detail::data_of<set_kind, K>
{
    using key_type = K;
};
/// A value of type V per key of type K: its elements are its keys, and it holds key and value pairs.
template<typename K, typename V>
struct map : detail::data_of<map_kind, K, V>
{
    using element_types = boost::mp11::mp_list<K, V>;
    using key_type = K;
    using mapped_type = V;
    using element_type = std::pair<const K, V>;
};
/// Values of type V kept by value, in order.
template<typename V>
struct multiset : detail::data_of<multiset_kind, V>
{
    using value_type = V;
};
/// Values of type V by position, addressed by P: a number, or an index (urd::index<T>, any key urd::index_of maps to
/// 0..n-1), so that a sequence parallel to the Ts a Store numbers is reached by their indices. P names no presence: a
/// sequence holds an element at every position below its size.
template<typename V, typename P = std::size_t>
struct sequence : detail::data_of<sequence_kind, V>
{
    static_assert(indexable<P>, "urd::sequence<V, P>: P addresses positions 0..n-1 (urd::index_of<P>)");
    using value_type = V;
    using position_type = P;
};
/// Values of type V reached by their front or top.
template<typename V>
struct queue : detail::data_of<queue_kind, V>
{
    using value_type = V;
};
/// Values of type V interned by handle, each distinct one once. H is the handle insert hands out: an urd::index, of V
/// by default, of any type where the pool names it (a pool of urd::data<T> handing out index<T>). Pools that number
/// their values serve it; a tree pool, whose handle is its own (a root and a size), serves only the default.
template<typename V, typename H = index<V>>
struct pool : detail::data_of<pool_kind, V>
{
    static_assert(is_index_type<H>, "urd::pool<V, H>: H, the handle a pool hands out, is an urd::index");
    using value_type = V;
    using handle_type = H;
};
/// Values of type V in slots, by handle.
template<typename V>
struct slots : detail::data_of<slots_kind, V>
{
    using value_type = V;
};
/// Keys of type K in disjoint classes.
template<typename K>
struct partition : detail::data_of<partition_kind, K>
{
    using key_type = K;
};

namespace detail
{

/// The questions a request of Kind answers (its own, then every kind's), and its Store's facets (every kind's, then
/// its own).
template<typename Kind>
using format_of = boost::mp11::mp_append<typename Kind::format, typename any_kind::common_format>;
template<typename Kind>
using facets_of = typename join_facets<typename any_kind::common_facets, typename Kind::facets>::type;

/// Whether a Store of Kind keeps its elements where they are put (it lists appending, as a sequence does), rather than
/// placing them by their value or key.
template<typename Kind>
inline constexpr bool appends = listed_facet<facet::appending, facets_of<Kind>>;

}  // namespace detail

/// Whether a request of kind Kind contradicts itself, whatever container would hold it: by its kind's rule, and
/// order_by<C> where its kind orders by nothing it asks for. Anything else no container serves fails in the registry.
template<typename Kind>
constexpr bool well_formed(const property_set& stated)
{ return Kind::well_formed(stated) && (!stated.has(comparison::given) || Kind::accepts_order_by(stated)); }

}  // namespace urd
