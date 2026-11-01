#pragma once

// The roles of a request (urd/registry/facts.hpp): what its spec states, and the data tags of its elements, keys,
// values and rows and of its order (urd/data_tags.hpp), computed once per spec; the registry matches containers'
// tags against them (urd/registry/scan.hpp).

#include "urd/data_tags.hpp"
#include "urd/registry/facts.hpp"
#include "urd/semantics/less.hpp"

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
