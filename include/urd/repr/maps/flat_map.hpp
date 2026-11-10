#pragma once

// Boost's flat map and multimap (a sorted vector of entries), defined. Include it where a Store resolves to one.

#include "urd/registry/declaration.hpp"
#include "urd/repr/core/boost_memory.hpp"
#include "urd/repr/maps/flat_map_fwd.hpp"
#include "urd/traits.hpp"

#include <boost/container/flat_map.hpp>

template<> struct urd::include<"urd/repr/maps/flat_map.hpp"> {};

namespace urd::detail
{

template<typename S>
struct representation_of<flat_map_decl, S>
{
    using type = boost::container::flat_map<typename S::key_type, typename S::mapped_type, typename S::order_type>;
};
template<typename S>
struct representation_of<flat_multimap_decl, S>
{
    using type = boost::container::flat_multimap<typename S::key_type, typename S::mapped_type, typename S::order_type>;
};

}  // namespace urd::detail
