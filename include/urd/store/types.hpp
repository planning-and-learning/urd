#pragma once

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/kinds.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <type_traits>
#include <utility>

namespace urd::detail
{

// A Store is its core (store.hpp) and facets (namespace facet): one class per group of properties, holding
// the members those properties allow, inherited where the Store's properties switch them on. Within one Store each
// member name lives in one facet only, so that calls are never ambiguous between two of them; across kinds a name may
// recur (insert in growth, pool, slots and grouping).

/// What a Store's interface lets change, from what its tags say (urd::tag_defaults): nothing where one stops it growing
/// (frozen, fixed_shape); inserts only where one stops it shrinking (monotone, nonempty); else inserts, erase and clear.
template<typename S>
inline constexpr bool grows = !traits_t<S>::urd_properties.template any_says<&category_table::stops_growth>();
template<typename S>
inline constexpr bool shrinks = grows<S> && !traits_t<S>::urd_properties.template any_says<&category_table::stops_shrinking>();

/// The types a Store and its facets share, whatever its kind. They come from the spec S, not from the Store,
/// which is still incomplete while its facets are built.
template<typename R, typename S>
struct store_types
{
    /// Elements change in place: its kind keeps them where they are put (a sequence: nothing depends on their values),
    /// and it is not frozen.
    static constexpr bool writable = appends<typename S::kind> && !S::urd_properties.template any_says<&category_table::stops_changes>();
    /// Iterated in an order the request asked for.
    static constexpr bool ordered = S::iterable && S::urd_properties.template any_says<&category_table::fixes_iteration_order>();

    using value_type = typename S::element_type;
    /// What lookups take, as the kind's data says: a map's key, else an element.
    using lookup_type = typename S::lookup_type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
};

/// The types of an iterable Store's iterators and references, which come from the representation's. Only the
/// facets of iterable Stores form them, so a representation without iterators never has to name any.
template<typename R, typename S>
struct iterator_types
{
    // A row of a nested Store may come as a view of flattened storage, so the references are the representation's.
    using const_reference = std::ranges::range_reference_t<const R>;
    using reference = std::conditional_t<store_types<R, S>::writable, std::ranges::range_reference_t<R>, const_reference>;
    using const_iterator = std::ranges::iterator_t<const R>;
    using iterator = std::conditional_t<store_types<R, S>::writable, std::ranges::iterator_t<R>, const_iterator>;
};

/// Whether a Store compares in order: it has an order, and the elements its representation yields compare
/// (a flattened Store's rows are slices, which do not).
template<typename R, typename S>
consteval bool compares_in_order()
{
    if constexpr (store_types<R, S>::ordered)
        return std::three_way_comparable<std::ranges::range_value_t<R>>;
    else
        return false;
}

/// When a Store has facet F, declared beside the facet (urd/store/<facet>.hpp): `on<R, S>()`, asked only of the facets
/// its kind lists (urd/kinds.hpp). Most read only the spec S; with R void, it is asked from the spec alone, before a
/// container is chosen.
template<template<typename, typename, typename> class F>
struct facet_condition;

/// Whether a pool's values are rows: Stores or registered containers holding elements (held_as, as the pools'
/// containers read them). A Store is not completed to ask (its container's header may be absent here).
template<typename V>
consteval bool holds_rows()
{ return held_as<V>::store; }

/// The operations a facet offers (urd::operation, costs.hpp), declared beside the facet as an operation_list: what a Store's container is
/// asked to state a cost for, and what a search knows a kind offers. None by default.
template<template<typename, typename, typename> class F>
struct facet_operations
{
    using offered = operation_list<>;
};

/// Whether a Store for spec S over representation R has facet F: its kind lists F, and F's condition holds.
template<typename R, typename S, template<typename, typename, typename> class F>
consteval bool facet_on()
{
    if constexpr (listed_facet<F, facets_of<typename S::kind>>)
        return facet_condition<F>::template on<R, S>();
    else
        return false;
}

/// How facets reach the Store's representation, which is private to the Store.
struct store_access
{
    template<typename Store>
    static auto& representation(Store& store)
    { return store.m_representation; }
    template<typename Store>
    static const auto& representation(const Store& store)
    { return store.m_representation; }
};

/// A facet where On, else nothing: one empty type per facet, so that several left-off facets are distinct bases. A
/// partial specialization, not std::conditional over an empty stand-in: one instantiation per facet instead of two,
/// which adds up over the twenty-odd facets of every Store.
template<bool On, typename Facet>
struct facet_if : Facet
{
};
template<typename Facet>
struct facet_if<false, Facet>
{
};

/// The facets a Store inherits: those its kind lists (urd/kinds.hpp) whose conditions hold.
template<typename Derived, typename R, typename S, typename List>
struct facet_bases;
template<typename Derived, typename R, typename S, template<typename, typename, typename> class... Fs>
struct facet_bases<Derived, R, S, facet_list<Fs...>> : facet_if<facet_condition<Fs>::template on<R, S>(), Fs<Derived, R, S>>...
{
};

}  // namespace urd::detail
