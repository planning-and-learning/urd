#pragma once

// Stores represented by urd/repr/values/parallel_hash_set.hpp convert to and from Python (urd/python/caster.hpp).

#include "urd/python/caster.hpp"
#include "urd/repr/values/parallel_hash_set.hpp"

namespace urd::python
{

template<typename T, typename Hash, typename Equal, typename Allocator, std::size_t N, typename Mutex>
inline constexpr bool casts<gtl::parallel_flat_hash_set<T, Hash, Equal, Allocator, N, Mutex>> = true;
template<typename T, typename Hash, typename Equal, typename Allocator, std::size_t N, typename Mutex>
inline constexpr bool casts<gtl::parallel_node_hash_set<T, Hash, Equal, Allocator, N, Mutex>> = true;

}  // namespace urd::python
