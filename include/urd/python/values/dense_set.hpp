#pragma once

// Stores represented by urd/repr/values/dense_set.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/values/dense_set.hpp"

namespace urd::python
{

template<typename K, bool TrackWords>
inline constexpr bool casts<repr::DenseSet<K, TrackWords>> = true;

}  // namespace urd::python
