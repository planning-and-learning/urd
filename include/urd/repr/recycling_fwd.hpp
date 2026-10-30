#pragma once

// Node containers that keep their erased nodes for reuse, declared with what the registry reads of them. Defined in
// urd/repr/recycling.hpp, which urd/use/nodes.hpp includes.

#include "urd/elements.hpp"
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
