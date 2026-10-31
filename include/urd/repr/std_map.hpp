#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <functional>
#include <map>
#include <unordered_map>

namespace urd
{

// The standard library's maps, as its sets: a tree is sorted by its natural order on orderable keys and frees the node
// of what it erases; a hash table on hashable keys frees its nodes but keeps its buckets. References survive
// inserts and rehashes.
template<typename K, typename V, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<std::map<K, V, C, Allocator>> : properties_of<sorted, unique, functional, stable_refs, sparse, shrinkable, reclaims, proportional_clear>
{
};

template<typename K, typename V, typename C, typename Allocator>
    requires natural_order<C, K>
struct container_traits<std::multimap<K, V, C, Allocator>> : properties_of<sorted, multi, stable_refs, shrinkable, reclaims, proportional_clear>
{
};

template<typename K, typename V, typename H, typename E, typename Allocator>
    requires natural_hash<H, E, K>
struct container_traits<std::unordered_map<K, V, H, E, Allocator>> : properties_of<unique, functional, stable_refs, sparse, shrinkable>
{
};

template<typename K, typename V, typename H, typename E, typename Allocator>
    requires natural_hash<H, E, K>
struct container_traits<std::unordered_multimap<K, V, H, E, Allocator>> : properties_of<multi, stable_refs, shrinkable>
{
};

template<typename K, typename V, typename C, typename Allocator>
inline constexpr auto defined_in<std::map<K, V, C, Allocator>> = detail::fixed_string("urd/use/nodes.hpp");
template<typename K, typename V, typename C, typename Allocator>
inline constexpr auto defined_in<std::multimap<K, V, C, Allocator>> = detail::fixed_string("urd/use/nodes.hpp");
template<typename K, typename V, typename H, typename E, typename Allocator>
inline constexpr auto defined_in<std::unordered_map<K, V, H, E, Allocator>> = detail::fixed_string("urd/use/nodes.hpp");
template<typename K, typename V, typename H, typename E, typename Allocator>
inline constexpr auto defined_in<std::unordered_multimap<K, V, H, E, Allocator>> = detail::fixed_string("urd/use/nodes.hpp");

}  // namespace urd

namespace urd
{

/// map.
template<typename K, typename V, typename C, typename A>
struct container_costs<std::map<K, V, C, A>>
{
    static constexpr cost insert = costs::log_n;
    static constexpr cost lookup = costs::log_n;
    static constexpr cost ordered_lookup = costs::log_n;
    static constexpr cost erase = costs::log_n;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 5;
};

/// multimap.
template<typename K, typename V, typename C, typename A>
struct container_costs<std::multimap<K, V, C, A>>
{
    static constexpr cost insert = costs::log_n;
    static constexpr cost lookup = costs::log_n;
    static constexpr cost ordered_lookup = costs::log_n;
    static constexpr cost erase = costs::log_n;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 5;
};

/// unordered_map.
template<typename K, typename V, typename H, typename E, typename A>
struct container_costs<std::unordered_map<K, V, H, E, A>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::one_e;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 4;
};

/// unordered_multimap.
template<typename K, typename V, typename H, typename E, typename A>
struct container_costs<std::unordered_multimap<K, V, H, E, A>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::lin_row_e;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 4;
};

}  // namespace urd

namespace urd
{

template<typename K, typename V, typename C, typename A>
struct container_synopsis<std::map<K, V, C, A>>
{
    static constexpr std::string_view id = "map";
    static constexpr std::string_view description = "std::map (balanced tree)";
    static constexpr std::string_view holds = "a node per entry; reclaims";
};

template<typename K, typename V, typename C, typename A>
struct container_synopsis<std::multimap<K, V, C, A>>
{
    static constexpr std::string_view id = "multimap";
    static constexpr std::string_view description = "std::multimap (balanced tree)";
    static constexpr std::string_view holds = "a node per entry; reclaims";
};

template<typename K, typename V, typename H, typename E, typename A>
struct container_synopsis<std::unordered_map<K, V, H, E, A>>
{
    static constexpr std::string_view id = "unordered_map";
    static constexpr std::string_view description = "std::unordered_map (hash table)";
    static constexpr std::string_view holds = "a node per entry + buckets; stable references";
};

template<typename K, typename V, typename H, typename E, typename A>
struct container_synopsis<std::unordered_multimap<K, V, H, E, A>>
{
    static constexpr std::string_view id = "unordered_multimap";
    static constexpr std::string_view description = "std::unordered_multimap (hash table)";
    static constexpr std::string_view holds = "a node per entry + buckets; stable references";
};

}  // namespace urd
