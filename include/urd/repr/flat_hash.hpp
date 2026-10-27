#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>

#include <functional>

namespace urd
{

// Boost's open-addressing hash tables, with std::hash on hashable elements. Elements live in one array of
// slots, so a rehash moves them (no stable_refs); erase and clear keep the slots.
template<hashable T, typename Allocator>
struct container_traits<boost::unordered_flat_set<T, std::hash<T>, std::equal_to<T>, Allocator>>
    : properties_of<unique, sparse, shrinkable, retains>
{
};

template<hashable K, typename V, typename Allocator>
struct container_traits<boost::unordered_flat_map<K, V, std::hash<K>, std::equal_to<K>, Allocator>>
    : properties_of<unique, sparse, shrinkable, retains>
{
};

}  // namespace urd
