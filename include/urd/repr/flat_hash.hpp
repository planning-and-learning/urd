#pragma once

#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/synopsis.hpp"
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

namespace urd
{

/// unordered_flat_set.
template<typename T, typename H, typename E, typename A>
struct container_costs<boost::unordered_flat_set<T, H, E, A>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::one_e;
    static constexpr cost clear = costs::lin_capacity;
    static constexpr cost iterate = costs::lin_capacity;
    static constexpr double words = 1.3;
};

/// unordered_flat_map.
template<typename K, typename V, typename H, typename E, typename A>
struct container_costs<boost::unordered_flat_map<K, V, H, E, A>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::one_e;
    static constexpr cost clear = costs::lin_capacity;
    static constexpr cost iterate = costs::lin_capacity;
    static constexpr double words = 1.3;
};

}  // namespace urd

namespace urd
{

template<typename T, typename H, typename E, typename A>
struct container_synopsis<boost::unordered_flat_set<T, H, E, A>>
{
    static constexpr std::string_view id = "unordered_flat_set";
    static constexpr std::string_view description = "boost::unordered_flat_set (open-addressing hash table)";
    static constexpr std::string_view holds = "~n/0.875 slots + 1 byte each; retains";
};

template<typename K, typename V, typename H, typename E, typename A>
struct container_synopsis<boost::unordered_flat_map<K, V, H, E, A>>
{
    static constexpr std::string_view id = "unordered_flat_map";
    static constexpr std::string_view description = "boost::unordered_flat_map (open-addressing hash table)";
    static constexpr std::string_view holds = "~n/0.875 slots + 1 byte each; retains";
};

}  // namespace urd
