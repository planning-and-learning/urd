#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <functional>
#include <set>
#include <unordered_set>

namespace urd
{

// std::set orders by its comparator; only urd::Less (or std::less, where it agrees) on orderable elements is the
// order that `sorted` means (natural_order). Hash tables likewise need urd's hash and equality, or the
// standard's where a type has them (natural_hash).
// References survive inserts and a hash table's rehash; iterators of a hash table do not. A tree frees the node of
// what it erases; a hash table frees its nodes but keeps its buckets, so it neither retains nor reclaims.
template<typename T, typename C, typename Allocator>
    requires natural_order<C, T>
struct container_traits<std::set<T, C, Allocator>> : properties_of<sorted, unique, stable_refs, sparse, shrinkable, reclaims>
{
};

template<typename T, typename C, typename Allocator>
    requires natural_order<C, T>
struct container_traits<std::multiset<T, C, Allocator>> : properties_of<sorted, stable_refs, shrinkable, reclaims>
{
};

template<typename T, typename H, typename E, typename Allocator>
    requires natural_hash<H, E, T>
struct container_traits<std::unordered_set<T, H, E, Allocator>> : properties_of<unique, stable_refs, sparse, shrinkable>
{
};

}  // namespace urd
