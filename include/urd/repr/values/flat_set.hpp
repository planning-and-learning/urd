#pragma once

// Boost's flat set and multiset (a sorted vector), defined. Include it where a Store resolves to one.

#include "urd/elements/hash.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/values/flat_set_fwd.hpp"
#include "urd/traits.hpp"

#include <boost/container/flat_set.hpp>

namespace urd
{

template<>
struct include<"urd/repr/values/flat_set.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<flat_set_decl, S>
{
    using type = boost::container::flat_set<typename S::element_type, typename S::order_type>;
};
template<typename S>
struct representation_of<flat_multiset_decl, S>
{
    using type = boost::container::flat_multiset<typename S::element_type, typename S::order_type>;
};

}  // namespace urd::detail

namespace urd
{

/// A flat set hashes as its keys, in order.
template<typename K, typename Compare, typename Allocator>
struct Hash<boost::container::flat_set<K, Compare, Allocator>>
{
    hash_t operator()(const boost::container::flat_set<K, Compare, Allocator>& set) const
        requires Hashable<K>
    { return urd::hash_range(set); }
};

}  // namespace urd
