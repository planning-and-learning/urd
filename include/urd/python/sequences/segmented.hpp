#pragma once

// Stores represented by urd/repr/sequences/segmented.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/sequences/segmented.hpp"

namespace urd::python
{

template<typename T, std::size_t FirstSegmentSize, bool ThreadSafe, std::unsigned_integral Index>
inline constexpr bool casts<repr::SegmentedVector<T, FirstSegmentSize, ThreadSafe, Index>> = true;

}  // namespace urd::python
