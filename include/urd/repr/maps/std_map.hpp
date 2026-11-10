#pragma once

#include "urd/cost.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/repr/core/std_memory.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <functional>
#include <map>
#include <unordered_map>

namespace urd
{

// The standard library's maps, as its sets: a tree is sorted by its natural order on orderable keys and frees the node
// of what it erases; a hash table on hashable keys frees its nodes but keeps its buckets. References survive
// inserts and rehashes.

}  // namespace urd

namespace urd
{

namespace detail
{

struct map_decl
{
    static constexpr std::string_view id = "map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/std_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides =
        property_set::of({ order::sorted, uniqueness::unique, referencing::stable_refs, keys::sparse, memory::reclaims, clearing::proportional_clear });
    static constexpr property_set relies_on = relies(traits<role::order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::log_n>,
                                       cost_entry<operation::lookup, costs::log_n>,
                                       cost_entry<operation::ordered_lookup, costs::log_n>,
                                       cost_entry<operation::erase, costs::log_n>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>>;
    static constexpr double words = 5;
    static constexpr std::string_view description = "std::map (balanced tree)";
    static constexpr std::string_view holds = "a node per entry; reclaims";
};
template<typename K, typename V, typename C, typename A>
struct declaration_of<std::map<K, V, C, A>>
{
    using declaration = map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>, C>();
};

}  // namespace detail

namespace detail
{

struct multimap_decl
{
    static constexpr std::string_view id = "multimap";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/std_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides =
        property_set::of({ order::sorted, multiplicity::multi, referencing::stable_refs, memory::reclaims, clearing::proportional_clear });
    static constexpr property_set relies_on = relies(traits<role::order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::log_n>,
                                       cost_entry<operation::lookup, costs::log_n>,
                                       cost_entry<operation::ordered_lookup, costs::log_n>,
                                       cost_entry<operation::erase, costs::log_n>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>>;
    static constexpr double words = 5;
    static constexpr std::string_view description = "std::multimap (balanced tree)";
    static constexpr std::string_view holds = "a node per entry; reclaims";
};
template<typename K, typename V, typename C, typename A>
struct declaration_of<std::multimap<K, V, C, A>>
{
    using declaration = multimap_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>, C>();
};

}  // namespace detail

namespace detail
{

struct unordered_map_decl
{
    static constexpr std::string_view id = "unordered_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/std_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, referencing::stable_refs, keys::sparse });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_ae>,
                                       cost_entry<operation::lookup, costs::one_e>,
                                       cost_entry<operation::erase, costs::one_e>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>>;
    static constexpr double words = 5;
    static constexpr std::string_view description = "std::unordered_map (hash table)";
    static constexpr std::string_view holds = "a node per entry + buckets; stable references";
};
template<typename K, typename V, typename H, typename E, typename A>
struct declaration_of<std::unordered_map<K, V, H, E, A>>
{
    using declaration = unordered_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

namespace detail
{

struct unordered_multimap_decl
{
    static constexpr std::string_view id = "unordered_multimap";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/std_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ multiplicity::multi, referencing::stable_refs });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_ae>,
                                       cost_entry<operation::lookup, costs::one_e>,
                                       cost_entry<operation::erase, costs::lin_row_e>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>>;
    static constexpr double words = 4;
    static constexpr std::string_view description = "std::unordered_multimap (hash table)";
    static constexpr std::string_view holds = "a node per entry + buckets; stable references";
};
template<typename K, typename V, typename H, typename E, typename A>
struct declaration_of<std::unordered_multimap<K, V, H, E, A>>
{
    using declaration = unordered_multimap_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

}  // namespace urd

template<> struct urd::include<"urd/repr/maps/std_map.hpp"> {};

namespace urd::detail
{

template<typename S>
struct representation_of<unordered_map_decl, S>
{
    using type = std::unordered_map<typename S::key_type, typename S::mapped_type, urd::Hash<typename S::key_type>, urd::EqualTo<typename S::key_type>>;
};
template<typename S>
struct representation_of<unordered_multimap_decl, S>
{
    using type = std::unordered_multimap<typename S::key_type, typename S::mapped_type, urd::Hash<typename S::key_type>, urd::EqualTo<typename S::key_type>>;
};
template<typename S>
struct representation_of<map_decl, S>
{
    using type = std::map<typename S::key_type, typename S::mapped_type, typename S::order_type>;
};
template<typename S>
struct representation_of<multimap_decl, S>
{
    using type = std::multimap<typename S::key_type, typename S::mapped_type, typename S::order_type>;
};

}  // namespace urd::detail
