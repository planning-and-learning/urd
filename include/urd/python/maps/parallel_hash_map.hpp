#pragma once

// Stores represented by urd/repr/maps/parallel_hash_map.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/maps/parallel_hash_map.hpp"

namespace urd::python
{

template<typename K, typename V>
inline constexpr bool casts<repr::ParallelHashMap<K, V>> = true;
template<typename K, typename V>
inline constexpr bool casts<repr::ParallelNodeHashMap<K, V>> = true;

}  // namespace urd::python
