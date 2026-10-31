#pragma once

#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/synopsis.hpp"
#include "urd/traits.hpp"

#include <functional>
#include <set>
#include <unordered_set>

namespace urd
{

// std::set orders by its comparator; only urd::Less (or std::less, where it agrees) on orderable elements is the
// order that `sorted` means (natural_order). Hash tables likewise need urd's hash and equality, or the
// standard's where a type has them (natural_hash).
// References survive inserts and a hash table's rehash; iterators of a hash table do not. A tree frees the node of
// what it erases; a hash table frees its nodes but keeps its buckets, so it neither retains nor reclaims.
template<typename T, typename C, typename Allocator>
    requires natural_order<C, T>
struct container_traits<std::set<T, C, Allocator>> : properties_of<sorted, unique, stable_refs, sparse, shrinkable, reclaims, proportional_clear>
{
};

template<typename T, typename C, typename Allocator>
    requires natural_order<C, T>
struct container_traits<std::multiset<T, C, Allocator>> : properties_of<sorted, stable_refs, shrinkable, reclaims, proportional_clear>
{
};

template<typename T, typename H, typename E, typename Allocator>
    requires natural_hash<H, E, T>
struct container_traits<std::unordered_set<T, H, E, Allocator>> : properties_of<unique, stable_refs, sparse, shrinkable>
{
};

template<typename T, typename C, typename Allocator>
inline constexpr auto defined_in<std::set<T, C, Allocator>> = detail::fixed_string("urd/use/nodes.hpp");
template<typename T, typename C, typename Allocator>
inline constexpr auto defined_in<std::multiset<T, C, Allocator>> = detail::fixed_string("urd/use/nodes.hpp");
template<typename T, typename H, typename E, typename Allocator>
inline constexpr auto defined_in<std::unordered_set<T, H, E, Allocator>> = detail::fixed_string("urd/use/nodes.hpp");

}  // namespace urd

namespace urd
{

/// set.
template<typename T, typename C, typename A>
struct container_costs<std::set<T, C, A>>
{
    static constexpr cost insert = costs::log_n;
    static constexpr cost lookup = costs::log_n;
    static constexpr cost ordered_lookup = costs::log_n;
    static constexpr cost erase = costs::log_n;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 5;
};

/// unordered_set.
template<typename T, typename H, typename E, typename A>
struct container_costs<std::unordered_set<T, H, E, A>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::one_e;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 4;
};

}  // namespace urd

namespace urd
{

template<typename T, typename C, typename A>
struct container_synopsis<std::set<T, C, A>>
{
    static constexpr std::string_view id = "set";
    static constexpr std::string_view description = "std::set (balanced tree)";
    static constexpr std::string_view holds = "a node per element; reclaims";
};

template<typename T, typename H, typename E, typename A>
struct container_synopsis<std::unordered_set<T, H, E, A>>
{
    static constexpr std::string_view id = "unordered_set";
    static constexpr std::string_view description = "std::unordered_set (hash table)";
    static constexpr std::string_view holds = "a node per element + buckets; stable references";
};

template<typename T, typename C, typename A>
struct container_synopsis<std::multiset<T, C, A>>
{
    static constexpr std::string_view id = "multiset";
    static constexpr std::string_view description = "std::multiset (balanced tree)";
    static constexpr std::string_view holds = "a node per element; reclaims";
};

}  // namespace urd
