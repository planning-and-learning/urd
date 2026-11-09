#pragma once

// Stores represented by urd/repr/values/unordered_flat_set.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"

namespace urd::python
{

template<typename T, typename Hash, typename Equal, typename Allocator>
inline constexpr bool casts<gtl::flat_hash_set<T, Hash, Equal, Allocator>> = true;

}  // namespace urd::python
