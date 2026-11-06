#pragma once

// Stores represented by urd/repr/values/sparse_set.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/values/sparse_set.hpp"

namespace urd::python
{

template<typename K, bool KeepOrder>
inline constexpr bool casts<repr::SparseSet<K, KeepOrder>> = true;

}  // namespace urd::python
