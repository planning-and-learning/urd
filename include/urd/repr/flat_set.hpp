#pragma once

// Boost's flat set and multiset (a sorted vector), defined. Include it where a Store resolves to one.

#include "urd/registry/row.hpp"
#include "urd/repr/flat.hpp"
#include "urd/traits.hpp"

#include <boost/container/flat_set.hpp>

namespace urd
{

template<>
struct include<"urd/repr/flat_set.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<"flat_set", S>
{
    using type = boost::container::flat_set<typename S::element_type, element_order_t<S>>;
};
template<typename S>
struct representation_of<"flat_multiset", S>
{
    using type = boost::container::flat_multiset<typename S::element_type, element_order_t<S>>;
};

}  // namespace urd::detail
