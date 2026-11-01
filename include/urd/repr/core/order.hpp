#pragma once

// The comparator a container that takes one is given for a request's spec S: what its kind's data says orders it
// (order_by<P>), else urd::Less over its elements, or its keys.

#include "urd/elements/less.hpp"

#include <type_traits>

namespace urd::detail
{

template<typename S>
using element_order_t = std::conditional_t<std::is_void_v<typename S::order_type>, Less<typename S::element_type>, typename S::order_type>;
template<typename S>
using key_order_t = std::conditional_t<std::is_void_v<typename S::order_type>, Less<typename S::key_type>, typename S::order_type>;

}  // namespace urd::detail
