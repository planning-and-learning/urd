#pragma once

// Stores represented by urd/repr/values/indexed.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/values/indexed.hpp"

namespace urd::python
{

template<typename T, typename Index, bool ThreadSafe, bool Stable>
inline constexpr bool casts<repr::IndexedSet<T, Index, ThreadSafe, Stable>> = true;

}  // namespace urd::python
