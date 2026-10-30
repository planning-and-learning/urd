#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <boost/unordered/unordered_flat_map_fwd.hpp>
#include <boost/unordered/unordered_flat_set_fwd.hpp>
#include <functional>

namespace urd
{

// Boost's open-addressing hash tables, with a natural hash on hashable elements. Elements live in one array of
// slots, so a rehash moves them (no stable_refs); erase and clear keep the slots. Declared here through Boost's
// forward headers; defined by urd/use/hash.hpp.
template<typename T, typename H, typename E, typename Allocator>
    requires natural_hash<H, E, T>
struct container_traits<boost::unordered_flat_set<T, H, E, Allocator>> : properties_of<unique, sparse, shrinkable, retains>
{
};

template<typename K, typename V, typename H, typename E, typename Allocator>
    requires natural_hash<H, E, K>
struct container_traits<boost::unordered_flat_map<K, V, H, E, Allocator>> : properties_of<unique, functional, sparse, shrinkable, retains>
{
};

template<typename T, typename H, typename E, typename Allocator>
inline constexpr auto defined_in<boost::unordered_flat_set<T, H, E, Allocator>> = detail::fixed_string("urd/use/hash.hpp");
template<typename K, typename V, typename H, typename E, typename Allocator>
inline constexpr auto defined_in<boost::unordered_flat_map<K, V, H, E, Allocator>> = detail::fixed_string("urd/use/hash.hpp");

}  // namespace urd
