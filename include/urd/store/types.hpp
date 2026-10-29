#pragma once

#include "urd/elements.hpp"
#include "urd/interface.hpp"
#include "urd/traits.hpp"

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <type_traits>

namespace urd::detail
{

// A Store is its core (store.hpp) and facets (namespace facet): one class per group of properties, holding
// the members those properties allow, inherited where the Store's properties switch them on. Within one Store each
// member name lives in one facet only, so that calls are never ambiguous between two of them; across kinds a name may
// recur (insert in growth, pool, slots and grouping), which store/denials.hpp records per name.

/// The types a Store and its facets share, whatever its kind. They come from the spec S, not from the Store,
/// which is still incomplete while its facets are built.
template<typename R, typename S>
struct store_types
{
    /// Kept by value or key, as an associative container (set, map, sorted sequence).
    static constexpr bool keyed = by_value<S>;
    /// Elements change in place: nothing depends on their values, and the Store may change.
    static constexpr bool writable = !keyed && !has<S, monotone>;
    /// Iterated in an order the request asked for.
    static constexpr bool ordered = S::iterable && (has<S, sorted> || has<S, insertion_ordered> || S::is_sequence);

    using value_type = typename S::element_type;
    using key_type = typename S::key_type;
    using mapped_type = typename S::mapped_type;
    /// What lookups take: a map's key, else an element.
    using lookup_type = std::conditional_t<S::is_map, key_type, value_type>;
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

/// The facets by name: which ones a Store inherits is read from facet_switches, by the Store's base list and by the
/// denials that stand in for the facets it lacks.
enum class facet_id : std::uint8_t
{
    sizing,
    iteration,
    equality,
    ordering,
    lookup,
    ordered_lookup,
    reverse_iteration,
    positions,
    contiguous_data,
    capacity,
    growth,
    removal,
    values_by_key,
    multi_values,
    set_algebra,
    queue_access,
    scopes,
    layers,
    changes,
    pool,
    records,
    slots,
    grouping,
    entries,
    inverse,
    shape,
    count
};

/// Which facets a Store for spec S over representation R has: its kind and properties switch them on.
template<typename R, typename S>
struct facet_switches
{
    static constexpr bool on(facet_id id)
    {
        switch (id)
        {
            case facet_id::sizing:
                return S::sized;
            case facet_id::iteration:
                return S::iterable;
            case facet_id::equality:
                return std::equality_comparable<typename S::element_type> && (std::equality_comparable<R> || S::iterable);
            case facet_id::ordering:
                return compares_in_order<R, S>();
            case facet_id::lookup:
                return store_types<R, S>::keyed && S::iterable;
            case facet_id::ordered_lookup:
                return has<S, sorted> && !is_entry_less<typename S::order_type>;
            case facet_id::reverse_iteration:
                return store_types<R, S>::ordered;
            case facet_id::positions:
                return has<S, positional>;
            case facet_id::contiguous_data:
                return has<S, contiguous>;
            case facet_id::capacity:
                return has<S, growable>;
            case facet_id::growth:
                return has<S, growable> && S::iterable;
            case facet_id::removal:
                return has<S, shrinkable> && S::iterable;
            case facet_id::values_by_key:
                return S::is_map && has<S, unique>;
            case facet_id::multi_values:
                return S::is_map && !has<S, unique>;
            case facet_id::set_algebra:
                return S::is_set;
            case facet_id::queue_access:
                return S::is_queue;
            case facet_id::scopes:
                return has<S, scoped>;
            case facet_id::layers:
                return has<S, layered>;
            case facet_id::changes:
                return has<S, change_log>;
            case facet_id::pool:
                return S::is_pool && has<S, unique> && !has<S, flat>;
            case facet_id::records:
                return S::is_pool && has<S, unique> && has<S, flat>;
            case facet_id::slots:
                return S::is_pool && !has<S, unique>;
            case facet_id::grouping:
                return S::is_partition;
            case facet_id::entries:
                return S::is_map && has<S, insertion_ordered>;
            case facet_id::inverse:
                return S::is_map && has<S, bidirectional>;
            case facet_id::shape:
                return has<S, fixed_shape>;
            case facet_id::count:
                return false;
        }
        return false;
    }
    /// Whether any of the facets is on: whether some facet declares a member name.
    template<std::size_t N>
    static constexpr bool any_on(const std::array<facet_id, N>& ids)
    {
        for (const auto id : ids)
            if (on(id))
                return true;
        return false;
    }
};

/// How facets reach the Store's representation, which is private to the Store.
struct store_access
{
    template<typename Store>
    static auto& representation(Store& store)
    {
        return store.m_representation;
    }
    template<typename Store>
    static const auto& representation(const Store& store)
    {
        return store.m_representation;
    }
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

}  // namespace urd::detail
