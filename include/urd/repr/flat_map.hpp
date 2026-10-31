#pragma once

// Boost's flat map and multimap (a sorted vector of entries), defined. Include it where a Store resolves to one.

#include "urd/registry/declaration.hpp"
#include "urd/repr/flat.hpp"
#include "urd/traits.hpp"

#include <boost/container/flat_map.hpp>

namespace urd
{

template<>
struct include<"urd/repr/flat_map.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<flat_map_decl, S>
{
    using type = boost::container::flat_map<typename S::key_type, typename S::mapped_type, key_order_t<S>>;
};
template<typename S>
struct representation_of<flat_multimap_decl, S>
{
    using type = boost::container::flat_multimap<typename S::key_type, typename S::mapped_type, key_order_t<S>>;
};

}  // namespace urd::detail
