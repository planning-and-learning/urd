#pragma once

// Stores represented by urd/repr/maps/std_map.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/maps/std_map.hpp"

namespace urd::python
{

template<typename K, typename V, typename Compare, typename Allocator>
inline constexpr bool casts<std::map<K, V, Compare, Allocator>> = true;
template<typename K, typename V, typename Hash, typename Equal, typename Allocator>
inline constexpr bool casts<std::unordered_map<K, V, Hash, Equal, Allocator>> = true;

}  // namespace urd::python
