#pragma once

#include "urd/cost.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/kinds.hpp"
#include "urd/repr/core/numbered.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

namespace urd::detail
{

// A Store is its core (store.hpp) and facets (namespace facet): one class per group of properties, holding
// the members those properties allow, inherited where the Store's properties switch them on. Within one Store a member
// name must live in one facet only, so that calls are never ambiguous between two of them; across kinds a name may
// recur (insert in insertion, appending, pool, records, slots and grouping). Within map_kind, entries shares names with
// positions (position) and inverse (keys): no container serves a request that has both.

/// What a Store's interface lets change, from what its tags say (urd::tag_defaults): nothing where one stops it growing
/// (frozen, fixed_shape); inserts only where one stops it shrinking (monotone, nonempty); else inserts, erase and clear.
/// clear() also where shrinking alone is stopped (monotone_until_clear): every tag that stops clearing stops shrinking.
template<typename S>
inline constexpr bool grows = !traits_t<S>::urd_properties.template any_says<&answer_says::stops_growth>();
template<typename S>
inline constexpr bool shrinks = grows<S> && !traits_t<S>::urd_properties.template any_says<&answer_says::stops_shrinking>();
template<typename S>
inline constexpr bool clears = grows<S> && !traits_t<S>::urd_properties.template any_says<&answer_says::stops_clearing>();
/// Whether a Store that shrinks also changes in the middle: inserts before, and erases, any position. A sequence whose
/// request says it changes at its ends only (urd::answer_says) removes at its end only: pop_back, clear, resize.
template<typename S>
inline constexpr bool changes_in_the_middle =
    shrinks<S>
    && !(std::same_as<typename S::kind, sequence_kind> && traits_t<S>::urd_properties.template any_says<&answer_says::changes_at_ends>());

/// What a Store and its facets share, whatever its kind: the facts its request decides, and its member types. They come
/// from the spec S alone, not from the Store, which is still incomplete while its facets are built.
template<typename S>
struct store_types
{
    /// What it holds may change after it is built: no tag stops that (frozen).
    static constexpr bool changeable = !S::urd_properties.template any_says<&answer_says::stops_changes>();
    /// Elements change in place: its kind keeps them where they are put (a sequence: nothing depends on their values),
    /// and it is not frozen.
    static constexpr bool writable = appends<typename S::kind> && changeable;
    /// What iterating gives may be changed in place: a writable sequence's elements, and a changeable map's values (its
    /// keys stay const, as in the standard's maps).
    static constexpr bool values_writable = writable || (std::same_as<typename S::kind, map_kind> && changeable);
    /// Iterated in an order the request fixes (sorted, insertion_ordered, a sequence): ordered by it.
    static constexpr bool ordered = S::iterable && S::urd_properties.template any_says<&answer_says::fixes_iteration_order>();
    /// Its contents fix the order its elements come in (sorted, canonical, a sequence): hashed by it.
    static constexpr bool hashed_in_order = S::iterable && S::urd_properties.template any_says<&answer_says::fixes_hash_order>();
    /// It must hold an element at least (nonempty): it has no default constructor, and what builds it must hold one.
    static constexpr bool needs_an_element = S::urd_properties.template any_says<&answer_says::needs_an_element>();
    /// Built with the bit width its elements fit in (urd::bit: urd::bit_width).
    static constexpr bool built_with_width = S::urd_properties.template any_says<&answer_says::built_with_width>();
    /// Built with the column widths its rows are stored by (urd::by_column_widths: urd::column_widths).
    static constexpr bool built_with_column_widths = S::urd_properties.template any_says<&answer_says::built_with_column_widths>();

    using value_type = typename S::element_type;
    /// What lookups take, as the kind's data says: a map's key, else an element.
    using lookup_type = typename S::lookup_type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
};

/// The types of an iterable Store's iterators and references, which come from the representation's. Only the
/// facets of iterable Stores form them, so a representation without iterators never has to name any.
/// Read from the representation's begin() and its iterators' own types (std::iterator_traits), not through std::ranges:
/// its concepts look functions up by the element type, which completes it, and a Store may be held while its element
/// type is incomplete (rule I5).
template<typename R, typename S>
struct iterator_types
{
    using const_iterator = decltype(std::declval<const R&>().begin());
    using iterator = std::conditional_t<store_types<S>::values_writable, decltype(std::declval<R&>().begin()), const_iterator>;
    // A row of a nested Store may come as a view of flattened storage, so the references are the representation's.
    using const_reference = typename std::iterator_traits<const_iterator>::reference;
    using reference = typename std::iterator_traits<iterator>::reference;
};

/// What every facet's condition says unless it says otherwise: each specialization derives from it. Whether the facet
/// hands out indices for views (records, insertion_positions), so that the Store's container is numbered (urd::layer).
struct facet_defaults
{
    static constexpr bool hands_out_indices = false;
};
/// When a Store has facet F: `on<R, S>()`, asked only of the facets its kind lists (urd/kinds.hpp). Always, unless
/// the facet's header (urd/store/<facet>.hpp) specializes it. With R void it is asked from the spec alone, before a
/// container is chosen (what a kind offers; whether its container is numbered).
template<template<typename, typename, typename> class F>
struct facet_condition : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return true; }
};

/// Whether a Store for spec S has a facet that hands out indices for views (facet_defaults::hands_out_indices), asked of
/// the facets its kind lists, each by its own condition from the spec alone.
template<typename S, template<typename, typename, typename> class F>
consteval bool hands_out_indices_by()
{
    if constexpr (facet_condition<F>::hands_out_indices)
        return facet_condition<F>::template on<void, S>();
    else
        return false;
}
template<typename S, template<typename, typename, typename> class... Fs>
consteval bool hands_out_indices(facet_list<Fs...> /*facets*/)
{ return (hands_out_indices_by<S, Fs>() || ...); }
/// The container R of a Store for spec S, numbered where the Store hands out indices for views (urd::layer).
template<typename S, typename R>
using numbered_t = std::conditional_t<hands_out_indices<S>(facets_of<typename S::kind> {}), repr::Numbered<R>, R>;

/// The operations a facet offers (urd::operation, cost.hpp), declared beside the facet as an operation_list: what a Store's container is
/// asked to state a cost for, and what a search knows a kind offers. None by default.
template<template<typename, typename, typename> class F>
struct facet_operations
{
    using offered = operation_list<>;
};

/// How facets reach the Store's representation, which is private to the Store: a Store's, or from within one of its
/// facets, `of<Derived>(*this)`.
struct store_access
{
    template<typename Store>
    static auto& representation(Store& store)
    { return store.m_representation; }
    template<typename Store>
    static const auto& representation(const Store& store)
    { return store.m_representation; }
    template<typename Derived, typename Facet>
    static auto& of(Facet& facet)
    { return representation(static_cast<Derived&>(facet)); }
    template<typename Derived, typename Facet>
    static const auto& of(const Facet& facet)
    { return representation(static_cast<const Derived&>(facet)); }
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
