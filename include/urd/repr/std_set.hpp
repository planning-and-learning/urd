#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <functional>
#include <set>
#include <unordered_set>

namespace urd
{

// std::set orders by its comparator; only std::less, on orderable elements, is the order that `sorted` means.
// References survive inserts and a hash table's rehash; iterators of a hash table do not.
template<orderable T, typename Allocator>
struct container_traits<std::set<T, std::less<T>, Allocator>> : properties_of<sorted, unique, stable_refs, sparse, shrinkable>
{
};

template<orderable T, typename Allocator>
struct container_traits<std::multiset<T, std::less<T>, Allocator>> : properties_of<sorted, stable_refs, shrinkable>
{
};

template<hashable T, typename Allocator>
struct container_traits<std::unordered_set<T, std::hash<T>, std::equal_to<T>, Allocator>> : properties_of<unique, stable_refs, sparse, shrinkable>
{
};

}  // namespace urd
