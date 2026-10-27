#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <functional>

namespace urd
{

// Boost's flat sets are sorted, as std::set is, by a natural order on orderable elements.
// Their vector keeps its capacity when elements are erased.
template<typename K, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<boost::container::flat_set<K, C, Allocator>> :
    properties_of<sorted, unique, sparse, contiguous, canonical, shrinkable, retains, proportional_clear>
{
};

template<typename K, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<boost::container::flat_multiset<K, C, Allocator>> :
    properties_of<sorted, contiguous, canonical, shrinkable, retains, proportional_clear>
{
};

// Their maps likewise, sorted by key. Their iterators would let a key be overwritten; a Store hands out only
// const ones.
template<typename K, typename V, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<boost::container::flat_map<K, V, C, Allocator>> :
    properties_of<sorted, unique, sparse, contiguous, canonical, shrinkable, retains, proportional_clear>
{
};

// A multimap keeps equal keys' values in insertion order, so equal contents inserted in another order differ:
// it is not canonical.
template<typename K, typename V, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<boost::container::flat_multimap<K, V, C, Allocator>> : properties_of<sorted, multi, contiguous, shrinkable, retains, proportional_clear>
{
};

}  // namespace urd
