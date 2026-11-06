#pragma once

// Stores represented by urd/repr/values/flat_set.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/values/flat_set.hpp"

namespace urd::python
{

template<typename T, typename Compare, typename Allocator>
inline constexpr bool casts<boost::container::flat_set<T, Compare, Allocator>> = true;

}  // namespace urd::python
