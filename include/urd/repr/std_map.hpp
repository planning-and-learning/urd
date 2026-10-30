#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <functional>
#include <map>
#include <unordered_map>

namespace urd
{

// The standard library's maps, as its sets: a tree is sorted by its natural order on orderable keys and frees the node
// of what it erases; a hash table on hashable keys frees its nodes but keeps its buckets. References survive
// inserts and rehashes.
template<typename K, typename V, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<std::map<K, V, C, Allocator>> : properties_of<sorted, unique, functional, stable_refs, sparse, shrinkable, reclaims, proportional_clear>
{
};

template<typename K, typename V, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<std::multimap<K, V, C, Allocator>> : properties_of<sorted, multi, stable_refs, shrinkable, reclaims, proportional_clear>
{
};

template<typename K, typename V, typename H, typename E, typename Allocator>
    requires natural_hash<H, E, K>
struct container_traits<std::unordered_map<K, V, H, E, Allocator>> : properties_of<unique, functional, stable_refs, sparse, shrinkable>
{
};

template<typename K, typename V, typename H, typename E, typename Allocator>
    requires natural_hash<H, E, K>
struct container_traits<std::unordered_multimap<K, V, H, E, Allocator>> : properties_of<multi, stable_refs, shrinkable>
{
};

}  // namespace urd
