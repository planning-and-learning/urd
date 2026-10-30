#pragma once

// The contiguous family: sorted containers in one array. Boost.Container's flat set, multiset, map and multimap, and
// the map whose entries are kept in an order that reads the values. Include it where a Store of one is held. Pulls in
// Boost.Container.

#include "urd/repr/entry_ordered.hpp"
#include "urd/repr/flat.hpp"
#include "urd/traits.hpp"

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

namespace urd
{

template<>
struct include<"urd/use/contiguous.hpp">
{
};

}  // namespace urd
