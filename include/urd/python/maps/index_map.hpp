#pragma once

// Stores represented by urd/repr/maps/index_map.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/maps/index_map.hpp"

namespace urd::python
{

template<typename K, typename V, bool Generations, bool ThreadSafe, bool Stable>
inline constexpr bool casts<repr::IndexMap<K, V, Generations, ThreadSafe, Stable>> = true;

}  // namespace urd::python
