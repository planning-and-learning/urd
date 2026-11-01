#pragma once

// A map with scopes (pop_scope undoes every change since the matching push_scope), declared with what the registry
// reads of it, over a flat hash table, a tree or a dense map. Defined in urd/repr/wrappers/scoped.hpp; the map it
// scopes in its own header.

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/repr/maps/dense_map_fwd.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/maps/unordered_flat_map_fwd.hpp"
#include "urd/repr/values/dense_set_fwd.hpp"
#include "urd/repr/values/unordered_flat_set_fwd.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <boost/unordered/unordered_flat_map_fwd.hpp>
#include <map>

namespace urd::repr
{

/// Map, with a log of each change made inside a scope, undone when the scope ends. The registry names it for any key
/// type, so it is an unconstrained template; its container_traits declare properties where Map's do.
template<typename Map>
class Scoped;

/// Scopes over a flat hash table.
template<typename K, typename V>
using ScopedMap = Scoped<boost::unordered_flat_map<K, V, Hash<K>, EqualTo<K>>>;
/// Scopes over a tree: sorted.
template<typename K, typename V>
using ScopedTreeMap = Scoped<std::map<K, V, Less<K>>>;
/// Scopes over a slot per index: dense keys.
template<typename K, typename V>
using ScopedDenseMap = Scoped<IndexMap<K, V, false>>;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// scoped map (a flat hash table, scopes undone by a log)
struct scoped_map_decl
{
    static constexpr std::string_view id = "scoped_map";
    static constexpr auto headers = header_list<2> { "urd/repr/maps/unordered_flat_map.hpp", "urd/repr/wrappers/scoped.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, keys::sparse, memory::retains, scoping::scoped });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = costs_over<unordered_flat_map_decl,
                                       cost_entry<operation::insert, costs::one_ae>,
                                       cost_entry<operation::lookup, costs::one_e>,
                                       cost_entry<operation::erase, costs::one_e>,
                                       cost_entry<operation::clear, costs::lin_capacity>,
                                       cost_entry<operation::iterate, costs::lin_capacity>,
                                       cost_entry<operation::scope, costs::lin_changes>>;
    static constexpr double words = 2.3;
    static constexpr std::string_view description = "scoped map (a flat hash table, scopes undone by a log)";
    static constexpr std::string_view holds = "+ a log entry per change; pop_scope undoes them";
};
template<typename K, typename V>
struct declaration_of<repr::ScopedMap<K, V>>
{
    using declaration = scoped_map_decl;
    static constexpr property_set roles = map_roles<K, V>();
};

}  // namespace detail

namespace detail
{

/// scoped tree map (std::map, scopes undone by a log)
struct scoped_tree_map_decl
{
    static constexpr std::string_view id = "scoped_tree_map";
    static constexpr auto headers = header_list<2> { "urd/repr/maps/std_map.hpp", "urd/repr/wrappers/scoped.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of(
        { order::sorted, uniqueness::unique, referencing::stable_refs, keys::sparse, memory::reclaims, clearing::proportional_clear, scoping::scoped });
    static constexpr property_set relies_on = relies(traits<role::order, trait::own_order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = costs_over<map_decl,
                                       cost_entry<operation::insert, costs::log_n>,
                                       cost_entry<operation::lookup, costs::log_n>,
                                       cost_entry<operation::ordered_lookup, costs::log_n>,
                                       cost_entry<operation::erase, costs::log_n>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>,
                                       cost_entry<operation::scope, costs::nlog_changes>>;
    static constexpr double words = 6;
    static constexpr std::string_view description = "scoped tree map (std::map, scopes undone by a log)";
    static constexpr std::string_view holds = "+ a log entry per change; pop_scope undoes them";
};
template<typename K, typename V>
struct declaration_of<repr::ScopedTreeMap<K, V>>
{
    using declaration = scoped_tree_map_decl;
    static constexpr property_set roles = map_roles<K, V>();
};

}  // namespace detail

namespace detail
{

/// scoped dense map (a slot per index, scopes undone by a log)
struct scoped_dense_map_decl
{
    static constexpr std::string_view id = "scoped_dense_map";
    static constexpr auto headers = header_list<2> { "urd/repr/maps/index_map.hpp", "urd/repr/wrappers/scoped.hpp" };
    using kind = map_kind;
    static constexpr property_set provides =
        property_set::of({ order::sorted, uniqueness::unique, keys::dense, memory::retains, referencing::none, scoping::scoped });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>(), traits<role::order, trait::own_order>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = costs_over<dense_map_decl,
                                       cost_entry<operation::insert, costs::one_a>,
                                       cost_entry<operation::lookup, costs::one>,
                                       cost_entry<operation::ordered_lookup, costs::lin_keys>,
                                       cost_entry<operation::erase, costs::one>,
                                       cost_entry<operation::clear, costs::lin_keys>,
                                       cost_entry<operation::iterate, costs::lin_keys>,
                                       cost_entry<operation::scope, costs::lin_changes>>;
    static constexpr double words = 2.2;
    static constexpr std::string_view description = "scoped dense map (a slot per index, scopes undone by a log)";
    static constexpr std::string_view holds = "+ a log entry per change; pop_scope undoes them";
};
template<typename K, typename V>
struct declaration_of<repr::ScopedDenseMap<K, V>>
{
    using declaration = scoped_dense_map_decl;
    static constexpr property_set roles = map_roles<K, V>();
};

}  // namespace detail

}  // namespace urd
