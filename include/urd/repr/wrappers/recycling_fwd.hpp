#pragma once

// Node containers that keep their erased nodes for reuse, declared with what the registry reads of them. Defined in
// urd/repr/wrappers/recycling.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/values/std_set.hpp"
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

namespace detail
{

/// recycling map (std::map that keeps erased nodes and their values for reuse)
struct recycling_map_decl
{
    static constexpr std::string_view id = "recycling_map";
    static constexpr auto headers = header_list<2> { "urd/repr/maps/std_map.hpp", "urd/repr/wrappers/recycling.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ order::sorted, uniqueness::unique, referencing::stable_refs, keys::sparse, memory::retains, clearing::proportional_clear });
    static constexpr role_tags relies_on = relies(traits<role::order, trait::natural_ordered>());
    static constexpr std::array<tag, 0> gives_elements {};
    struct operation_costs : map_decl::operation_costs
    {
        using part = map_decl;
        static constexpr cost clear = costs::lin_n;
        static constexpr double words = 5.5;
    };
    static constexpr std::string_view description = "recycling map (std::map that keeps erased nodes and their values for reuse)";
    static constexpr std::string_view holds = "erased nodes and their values kept for reuse";
};
template<typename K, typename V, typename C>
struct declaration_of<repr::Recycling<std::map<K, V, C>>>
{
    using declaration = recycling_map_decl;
    static constexpr role_tags roles = map_roles<K, V, C>();
};

}  // namespace detail

namespace detail
{

/// recycling hash set (std::unordered_set that keeps erased nodes for reuse)
struct recycling_hash_set_decl
{
    static constexpr std::string_view id = "recycling_hash_set";
    static constexpr auto headers = header_list<2> { "urd/repr/values/std_set.hpp", "urd/repr/wrappers/recycling.hpp" };
    using kind = set_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, referencing::stable_refs, keys::sparse, memory::retains });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs : unordered_set_decl::operation_costs
    {
        using part = unordered_set_decl;
        static constexpr cost clear = costs::lin_n;
        static constexpr double words = 4.5;
    };
    static constexpr std::string_view description = "recycling hash set (std::unordered_set that keeps erased nodes for reuse)";
    static constexpr std::string_view holds = "erased nodes kept for reuse";
};
template<typename T>
struct declaration_of<repr::RecyclingHashSet<T>>
{
    using declaration = recycling_hash_set_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// recycling tree set (std::set that keeps erased nodes for reuse)
struct recycling_tree_set_decl
{
    static constexpr std::string_view id = "recycling_tree_set";
    static constexpr auto headers = header_list<2> { "urd/repr/values/std_set.hpp", "urd/repr/wrappers/recycling.hpp" };
    using kind = set_kind;
    static constexpr property_set provides = property_set::of({ order::sorted, uniqueness::unique, referencing::stable_refs, keys::sparse, memory::retains, clearing::proportional_clear });
    static constexpr role_tags relies_on = relies(traits<role::order, trait::own_order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs : set_decl::operation_costs
    {
        using part = set_decl;
        static constexpr cost clear = costs::lin_n;
        static constexpr double words = 5.5;
    };
    static constexpr std::string_view description = "recycling tree set (std::set that keeps erased nodes for reuse)";
    static constexpr std::string_view holds = "erased nodes kept for reuse";
};
template<typename T>
struct declaration_of<repr::RecyclingTreeSet<T>>
{
    using declaration = recycling_tree_set_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// recycling hash map (std::unordered_map that keeps erased nodes and their values for reuse)
struct recycling_hash_map_decl
{
    static constexpr std::string_view id = "recycling_hash_map";
    static constexpr auto headers = header_list<2> { "urd/repr/maps/std_map.hpp", "urd/repr/wrappers/recycling.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, referencing::stable_refs, keys::sparse, memory::retains });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 0> gives_elements {};
    struct operation_costs : unordered_map_decl::operation_costs
    {
        using part = unordered_map_decl;
        static constexpr cost clear = costs::lin_n;
        static constexpr double words = 4.5;
    };
    static constexpr std::string_view description = "recycling hash map (std::unordered_map that keeps erased nodes and their values for reuse)";
    static constexpr std::string_view holds = "erased nodes and their values kept for reuse";
};
template<typename K, typename V>
struct declaration_of<repr::RecyclingHashMap<K, V>>
{
    using declaration = recycling_hash_map_decl;
    static constexpr role_tags roles = map_roles<K, V>();
};

}  // namespace detail

}  // namespace urd
