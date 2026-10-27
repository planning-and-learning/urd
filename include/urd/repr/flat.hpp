#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <functional>

namespace urd
{

// Boost's flat sets order by std::less by default; like std::set, only that, on orderable elements, is `sorted`.
// Their vector keeps its capacity when elements are erased.
template<orderable K, typename Allocator>
struct container_traits<boost::container::flat_set<K, std::less<K>, Allocator>> :
    properties_of<sorted, unique, sparse, contiguous, canonical, shrinkable, retains>
{
};

template<orderable K, typename Allocator>
struct container_traits<boost::container::flat_multiset<K, std::less<K>, Allocator>> : properties_of<sorted, contiguous, canonical, shrinkable, retains>
{
};

// Their maps likewise, sorted by key. Their iterators would let a key be overwritten; a Store hands out only
// const ones.
template<orderable K, typename V, typename Allocator>
struct container_traits<boost::container::flat_map<K, V, std::less<K>, Allocator>> :
    properties_of<sorted, unique, sparse, contiguous, canonical, shrinkable, retains>
{
};

// A multimap keeps equal keys' values in insertion order, so equal contents inserted in another order differ:
// it is not canonical.
template<orderable K, typename V, typename Allocator>
struct container_traits<boost::container::flat_multimap<K, V, std::less<K>, Allocator>> : properties_of<sorted, multi, contiguous, shrinkable, retains>
{
};

}  // namespace urd
