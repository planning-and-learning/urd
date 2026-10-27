#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <boost/container/flat_set.hpp>

#include <functional>

namespace urd
{

// Boost's flat sets order by std::less by default; like std::set, only that, on orderable elements, is `sorted`.
// Their vector keeps its capacity when elements are erased.
template<orderable K, typename Allocator>
struct container_traits<boost::container::flat_set<K, std::less<K>, Allocator>>
    : properties_of<sorted, unique, sparse, contiguous, canonical, shrinkable, retains>
{
};

template<orderable K, typename Allocator>
struct container_traits<boost::container::flat_multiset<K, std::less<K>, Allocator>>
    : properties_of<sorted, contiguous, canonical, shrinkable, retains>
{
};

}  // namespace urd
