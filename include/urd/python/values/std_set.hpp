#pragma once

// Stores represented by urd/repr/values/std_set.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/values/std_set.hpp"

namespace urd::python
{

template<typename T, typename Compare, typename Allocator>
inline constexpr bool casts<std::set<T, Compare, Allocator>> = true;
template<typename T, typename Hash, typename Equal, typename Allocator>
inline constexpr bool casts<std::unordered_set<T, Hash, Equal, Allocator>> = true;

}  // namespace urd::python
