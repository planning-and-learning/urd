#pragma once

// The nodes family: the standard's node containers, a node per element, so that references hold. std::set,
// multiset, map, multimap, unordered_set, unordered_map, unordered_multimap, and the recycling wrappers that keep
// erased nodes for the next elements. Include it where a Store of one is held. Pulls in nothing beyond the standard
// library.

#include "urd/repr/recycling.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/traits.hpp"

#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace urd
{

template<>
struct include<"urd/use/nodes.hpp">
{
};

}  // namespace urd
