#pragma once

// Stores represented by urd/repr/maps/ordered_map.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/maps/ordered_map.hpp"

namespace urd::python
{

template<typename Keys, typename V>
inline constexpr bool casts<repr::OrderedMap<Keys, V>> = true;

}  // namespace urd::python
