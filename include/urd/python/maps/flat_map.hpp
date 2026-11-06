#pragma once

// Stores represented by urd/repr/maps/flat_map.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/maps/flat_map.hpp"

namespace urd::python
{

template<typename K, typename V, typename Compare, typename Allocator>
inline constexpr bool casts<boost::container::flat_map<K, V, Compare, Allocator>> = true;

}  // namespace urd::python
