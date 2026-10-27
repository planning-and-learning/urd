#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <functional>
#include <map>
#include <unordered_map>

namespace urd
{

// The standard library's maps, as its sets: a tree is sorted by std::less on orderable keys and frees the node
// of what it erases; a hash table on hashable keys frees its nodes but keeps its buckets. References survive
// inserts and rehashes.
template<orderable K, typename V, typename Allocator>
struct container_traits<std::map<K, V, std::less<K>, Allocator>> : properties_of<sorted, unique, stable_refs, sparse, shrinkable, reclaims>
{
};

template<orderable K, typename V, typename Allocator>
struct container_traits<std::multimap<K, V, std::less<K>, Allocator>> : properties_of<sorted, multi, stable_refs, shrinkable, reclaims>
{
};

template<hashable K, typename V, typename Allocator>
struct container_traits<std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, Allocator>>
    : properties_of<unique, stable_refs, sparse, shrinkable>
{
};

template<hashable K, typename V, typename Allocator>
struct container_traits<std::unordered_multimap<K, V, std::hash<K>, std::equal_to<K>, Allocator>>
    : properties_of<multi, stable_refs, shrinkable>
{
};

}  // namespace urd
