#pragma once

// Node containers that keep their erased nodes for reuse, declared with what the registry reads of them. Defined in
// urd/repr/recycling.hpp, which urd/use/nodes.hpp includes.

#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/synopsis.hpp"
#include "urd/traits.hpp"

#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace urd::repr
{

/// A map (std::map or std::unordered_map) whose erased nodes, values and all, serve the next keys.
template<typename Map>
class Recycling;
/// A set (std::set or std::unordered_set) whose erased nodes serve the next elements.
template<typename Set>
class RecyclingSet;

/// A sorted map that keeps erased values for reuse.
template<typename K, typename V>
using RecyclingMap = Recycling<std::map<K, V, Less<K>>>;
/// A hash map that keeps erased values for reuse.
template<typename K, typename V>
using RecyclingHashMap = Recycling<std::unordered_map<K, V, Hash<K>, EqualTo<K>>>;
/// A hash set whose erased nodes are kept for the next elements.
template<typename T>
using RecyclingHashSet = RecyclingSet<std::unordered_set<T, Hash<T>, EqualTo<T>>>;
/// A sorted set whose erased nodes are kept for the next elements.
template<typename T>
using RecyclingTreeSet = RecyclingSet<std::set<T, Less<T>>>;

}  // namespace urd::repr

namespace urd
{

/// As Map, but its erased nodes, and the values in them, are kept for the next keys.
template<typename Map>
    requires container_traits<Map>::declared
struct container_traits<repr::Recycling<Map>> : with_t<container_traits<Map>, retains>
{
};
template<typename Map>
struct parts<repr::Recycling<Map>>
{
    using type = detail::type_list<Map>;
};

/// Its values keep what they hold when their keys are erased: their own properties, memory included.
template<typename K, typename V, typename C, typename Allocator>
struct element_traits<repr::Recycling<std::map<K, V, C, Allocator>>> : container_traits<V>
{
};
template<typename K, typename V, typename H, typename E, typename Allocator>
struct element_traits<repr::Recycling<std::unordered_map<K, V, H, E, Allocator>>> : container_traits<V>
{
};

/// As Set, a node per element, but the erased nodes are kept for the next elements: stable references, retained.
template<typename Set>
    requires container_traits<Set>::declared && has<container_traits<Set>, unique> && has<container_traits<Set>, stable_refs>
struct container_traits<repr::RecyclingSet<Set>> : with_t<container_traits<Set>, retains>
{
};
template<typename Set>
struct parts<repr::RecyclingSet<Set>>
{
    using type = detail::type_list<Set>;
};

template<typename Map>
inline constexpr auto defined_in<repr::Recycling<Map>> = detail::fixed_string("urd/use/nodes.hpp");
template<typename Set>
inline constexpr auto defined_in<repr::RecyclingSet<Set>> = detail::fixed_string("urd/use/nodes.hpp");

}  // namespace urd

namespace urd
{

/// recycling_map: its part's costs, but for what it declares here.
template<typename K, typename V, typename C>
struct container_costs<repr::Recycling<std::map<K, V, C>>> : part_costs<part_of<repr::Recycling<std::map<K, V, C>>>>
{
    static constexpr cost clear = costs::lin_nt;
    static constexpr double words = 5.5;
};

/// recycling_hash_set: its part's costs, but for what it declares here.
template<typename T>
struct container_costs<repr::RecyclingHashSet<T>> : part_costs<part_of<repr::RecyclingHashSet<T>>>
{
    static constexpr cost clear = costs::lin_nt;
    static constexpr double words = 4.5;
};

/// recycling_tree_set: its part's costs, but for what it declares here.
template<typename T>
struct container_costs<repr::RecyclingTreeSet<T>> : part_costs<part_of<repr::RecyclingTreeSet<T>>>
{
    static constexpr cost clear = costs::lin_nt;
    static constexpr double words = 5.5;
};

/// recycling_hash_map: its part's costs, but for what it declares here.
template<typename K, typename V>
struct container_costs<repr::RecyclingHashMap<K, V>> : part_costs<part_of<repr::RecyclingHashMap<K, V>>>
{
    static constexpr cost clear = costs::lin_nt;
    static constexpr double words = 4.5;
};

}  // namespace urd

namespace urd
{

template<typename K, typename V, typename C>
struct container_synopsis<repr::Recycling<std::map<K, V, C>>>
{
    static constexpr std::string_view id = "recycling_map";
    static constexpr std::string_view description = "recycling map (std::map that keeps erased nodes and their values for reuse)";
    static constexpr std::string_view holds = "erased nodes and their values kept for reuse";
};

template<typename T>
struct container_synopsis<repr::RecyclingHashSet<T>>
{
    static constexpr std::string_view id = "recycling_hash_set";
    static constexpr std::string_view description = "recycling hash set (std::unordered_set that keeps erased nodes for reuse)";
    static constexpr std::string_view holds = "erased nodes kept for reuse";
};

template<typename T>
struct container_synopsis<repr::RecyclingTreeSet<T>>
{
    static constexpr std::string_view id = "recycling_tree_set";
    static constexpr std::string_view description = "recycling tree set (std::set that keeps erased nodes for reuse)";
    static constexpr std::string_view holds = "erased nodes kept for reuse";
};

template<typename K, typename V>
struct container_synopsis<repr::RecyclingHashMap<K, V>>
{
    static constexpr std::string_view id = "recycling_hash_map";
    static constexpr std::string_view description = "recycling hash map (std::unordered_map that keeps erased nodes and their values for reuse)";
    static constexpr std::string_view holds = "erased nodes and their values kept for reuse";
};

}  // namespace urd
