#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
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

namespace urd
{

/// flat_set.
template<typename T, typename C, typename A>
struct container_costs<boost::container::flat_set<T, C, A>>
{
    static constexpr cost insert = costs::lin_n;
    static constexpr cost lookup = costs::log_n;
    static constexpr cost ordered_lookup = costs::log_n;
    static constexpr cost erase = costs::lin_n;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 1.5;
};

/// flat_multiset.
template<typename T, typename C, typename A>
struct container_costs<boost::container::flat_multiset<T, C, A>>
{
    static constexpr cost insert = costs::lin_n;
    static constexpr cost lookup = costs::log_n;
    static constexpr cost ordered_lookup = costs::log_n;
    static constexpr cost erase = costs::lin_n;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 1.5;
};

/// flat_map.
template<typename K, typename V, typename C, typename A>
struct container_costs<boost::container::flat_map<K, V, C, A>>
{
    static constexpr cost insert = costs::lin_n;
    static constexpr cost lookup = costs::log_n;
    static constexpr cost ordered_lookup = costs::log_n;
    static constexpr cost erase = costs::lin_n;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 1.5;
};

/// flat_multimap.
template<typename K, typename V, typename C, typename A>
struct container_costs<boost::container::flat_multimap<K, V, C, A>>
{
    static constexpr cost insert = costs::lin_n;
    static constexpr cost lookup = costs::log_n;
    static constexpr cost ordered_lookup = costs::log_n;
    static constexpr cost erase = costs::lin_n;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 1.5;
};

}  // namespace urd

namespace urd
{

template<typename T, typename C, typename A>
struct container_synopsis<boost::container::flat_set<T, C, A>>
{
    static constexpr std::string_view id = "flat_set";
    static constexpr std::string_view description = "boost::container::flat_set (sorted vector)";
    static constexpr std::string_view holds = "n elements in one array; retains";
};

template<typename T, typename C, typename A>
struct container_synopsis<boost::container::flat_multiset<T, C, A>>
{
    static constexpr std::string_view id = "flat_multiset";
    static constexpr std::string_view description = "boost::container::flat_multiset (sorted vector)";
    static constexpr std::string_view holds = "n elements in one array; retains";
};

template<typename K, typename V, typename C, typename A>
struct container_synopsis<boost::container::flat_map<K, V, C, A>>
{
    static constexpr std::string_view id = "flat_map";
    static constexpr std::string_view description = "boost::container::flat_map (sorted vector)";
    static constexpr std::string_view holds = "n entries in one array; retains";
};

template<typename K, typename V, typename C, typename A>
struct container_synopsis<boost::container::flat_multimap<K, V, C, A>>
{
    static constexpr std::string_view id = "flat_multimap";
    static constexpr std::string_view description = "boost::container::flat_multimap (sorted vector)";
    static constexpr std::string_view holds = "n entries in one array; retains";
};

}  // namespace urd
