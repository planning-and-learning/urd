#pragma once

// The tags by role (role_tags) that a request promises and a container relies on: what a Store states, and what its
// data is (urd/data_tags.hpp), its elements, values and rows.

#include "urd/property_set.hpp"
#include "urd/semantics/less.hpp"
#include "urd/traits.hpp"

#include <array>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>

namespace urd::detail
{

/// Tags by role: a request's (what it states and what its data is) or a container's (what it relies on). A role a
/// kind has not holds the roots: nothing promised, nothing relied on.
struct role_tags
{
    property_set stated = property_set::roots();       ///< the Store's own tags
    property_set element = property_set::roots();      ///< its elements (a map's keys) and their order
    property_set mapped = property_set::roots();       ///< a map's values
    property_set row = property_set::roots();          ///< its elements' own tags, where they are Stores (rows)
    property_set row_element = property_set::roots();  ///< the rows' elements
};
/// Whether `held` promises, role by role, what `needed` relies on.
constexpr bool promises(const role_tags& held, const role_tags& needed)
{
    return promises(held.stated, needed.stated) && promises(held.element, needed.element) && promises(held.mapped, needed.mapped)
           && promises(held.row, needed.row) && promises(held.row_element, needed.row_element);
}

/// The comparator of a container that takes one: the request's order_by where it gives one, else urd::Less.
template<typename S>
using element_order_t = std::conditional_t<std::is_void_v<typename S::order_type>, Less<typename S::element_type>, typename S::order_type>;
template<typename S>
using key_order_t = std::conditional_t<std::is_void_v<typename S::order_type>, Less<typename S::key_type>, typename S::order_type>;

/// `base` with every tag of `listed` (any range of tags) put in its category.
constexpr property_set with(property_set base, const auto& listed)
{
    for (const auto each : listed)
        base.put(each);
    return base;
}

/// `base` with `listed` put in their categories.
constexpr property_set with(property_set base, std::initializer_list<tag> listed)
{
    for (const auto each : listed)
        base.put(each);
    return base;
}

}  // namespace urd::detail

