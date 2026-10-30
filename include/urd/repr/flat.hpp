#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <boost/container/container_fwd.hpp>
#include <functional>

namespace urd
{

// Boost's flat sets are sorted, as std::set is, by a natural order on orderable elements.
// Their vector keeps its capacity when elements are erased. Declared here through Boost's forward header; defined by
// urd/use/contiguous.hpp.
template<typename K, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<boost::container::flat_set<K, C, Allocator>> :
    properties_of<sorted, unique, positional, sparse, contiguous, canonical, shrinkable, retains, proportional_clear>
{
};

template<typename K, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<boost::container::flat_multiset<K, C, Allocator>> :
    properties_of<sorted, positional, contiguous, canonical, shrinkable, retains, proportional_clear>
{
};

// Their maps likewise, sorted by key. Their iterators would let a key be overwritten; a Store hands out only
// const ones.
template<typename K, typename V, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<boost::container::flat_map<K, V, C, Allocator>> :
    properties_of<sorted, unique, functional, positional, sparse, contiguous, canonical, shrinkable, retains, proportional_clear>
{
};

// A multimap keeps equal keys' values in insertion order, so equal contents inserted in another order differ:
// it is not canonical.
template<typename K, typename V, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<boost::container::flat_multimap<K, V, C, Allocator>> :
    properties_of<sorted, multi, positional, contiguous, shrinkable, retains, proportional_clear>
{
};

template<typename K, typename C, typename Allocator>
inline constexpr auto defined_in<boost::container::flat_set<K, C, Allocator>> = detail::fixed_string("urd/use/contiguous.hpp");
template<typename K, typename C, typename Allocator>
inline constexpr auto defined_in<boost::container::flat_multiset<K, C, Allocator>> = detail::fixed_string("urd/use/contiguous.hpp");
template<typename K, typename V, typename C, typename Allocator>
inline constexpr auto defined_in<boost::container::flat_map<K, V, C, Allocator>> = detail::fixed_string("urd/use/contiguous.hpp");
template<typename K, typename V, typename C, typename Allocator>
inline constexpr auto defined_in<boost::container::flat_multimap<K, V, C, Allocator>> = detail::fixed_string("urd/use/contiguous.hpp");

}  // namespace urd
