#pragma once

// Boost's flat set and multiset (a sorted vector), defined. Include it where a Store resolves to one.

#include "urd/elements/hash.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/core/boost_memory.hpp"
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

/// A flat set is a value range: hashed, compared and ordered as its keys, in order.
template<typename K, typename Compare, typename Allocator>
inline constexpr bool value_range<boost::container::flat_set<K, Compare, Allocator>> = true;

}  // namespace urd
