#pragma once

// Stores represented by urd/repr/sequences/sequence.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/sequences/sequence.hpp"

namespace urd::python
{

template<typename T, typename Allocator>
inline constexpr bool casts<std::vector<T, Allocator>> = true;

}  // namespace urd::python
