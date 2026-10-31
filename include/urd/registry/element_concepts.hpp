#pragma once

// What a container's constraints ask of a request's elements beyond urd/elements.hpp, declared apart from any
// container, so that the registry reads them (urd/registry/features.hpp) without declaring the containers they
// belong to: a record cista can serialize, a bucket queue's integral priority, rows a jagged array can hold.

#include "urd/elements.hpp"
#include "urd/interface.hpp"
#include "urd/traits.hpp"

#include <functional>
#include <type_traits>

namespace urd::repr
{

/// A record cista can serialize into the arena: hashable (its identity), and naming its members for cista
/// (cista_members() gives urd::Members<&T::a, …>). Never a Store, which asking would complete.
template<typename T>
concept flat_record = !is_store<T> && hashable<T> && requires { T::cista_members(); };

/// The integral priority a bucket queue files an element under: its projection where Compare orders by one, else the
/// element. A type computation, so that asking it of an element completes nothing.
template<typename T, typename Compare>
struct bucket_priority_of
{
    using type = T;
};
template<typename T, typename Compare>
    requires requires { Compare::projection; }
struct bucket_priority_of<T, Compare>
{
    using type = std::remove_cvref_t<std::invoke_result_t<decltype(Compare::projection), const T&>>;
};
template<typename T, typename Compare>
using bucket_priority_t = typename bucket_priority_of<T, Compare>::type;

}  // namespace urd::repr

namespace urd::detail
{

/// Whether Jagged can hold rows Inner: frozen rows, of elements or (at a deeper level) of rows in order.
template<typename Inner>
concept flattenable = is_store<Inner> && has<container_traits<Inner>, frozen>
                      && (!is_store<typename container_traits<Inner>::element_type> || !by_value<container_traits<Inner>>);

/// Whether Jagged can hold rows Inner that change in place: rows of elements, as a sequence, or a dense set's bits;
/// or fixed-shape rows of such rows, at any depth.
template<typename Inner>
consteval bool holds_fixed_rows()
{
    if constexpr (!is_store<Inner>)
        return false;
    else
    {
        using traits = container_traits<Inner>;
        using below = typename traits::element_type;
        if constexpr (has<traits, frozen>)
            return false;
        else if constexpr (!is_store<below>)
            return !by_value<traits> || (traits::is_set && has<traits, unique, dense>);
        else
            return has<traits, fixed_shape> && holds_fixed_rows<below>();
    }
}
template<typename Inner>
concept fixed_rows = holds_fixed_rows<Inner>();

}  // namespace urd::detail
