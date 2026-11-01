#pragma once

// The roles of a request (urd/registry/facts.hpp: role_tags): what its spec states, and by role (urd::role) the type
// traits of its elements, keys, values and rows, of its order, and its rows' own tags (urd/type_traits_of.hpp),
// computed once per spec; the registry matches containers' reliances against them (urd/registry/scan.hpp).

#include "urd/registry/facts.hpp"
#include "urd/semantics/less.hpp"
#include "urd/type_traits_of.hpp"

#include <type_traits>

namespace urd::detail
{

/// The comparator spec S orders by: its order_by, else urd::Less over what it orders (a map's keys, else its
/// elements).
template<typename S>
using spec_order_t = std::conditional_t<std::is_void_v<typename S::order_type>,
                                        Less<std::conditional_t<std::is_void_v<typename S::mapped_type>, typename S::element_type, typename S::key_type>>,
                                        typename S::order_type>;

/// The roles of spec S: what it promises, and what its data and its order are.
template<typename S>
consteval role_tags request_roles()
{
    if constexpr (!std::is_void_v<typename S::mapped_type>)
        return map_roles<typename S::key_type, typename S::mapped_type, spec_order_t<S>>(S::promised);
    else
        return element_roles<typename S::element_type, spec_order_t<S>>(S::promised);
}

}  // namespace urd::detail
