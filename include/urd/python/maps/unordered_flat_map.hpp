#pragma once

// Stores represented by urd/repr/maps/unordered_flat_map.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"

namespace urd::python
{

template<typename K, typename V, typename Hash, typename Equal, typename Allocator>
inline constexpr bool casts<gtl::flat_hash_map<K, V, Hash, Equal, Allocator>> = true;

}  // namespace urd::python
