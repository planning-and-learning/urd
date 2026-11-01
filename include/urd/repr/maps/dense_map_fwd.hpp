#pragma once

// Containers for keys that are indices 0..n-1 (dense keys), declared with what the registry reads of them: a bitset,
// a sparse set, a slot-per-index map. Defined in urd/repr/values/dense_set.hpp, sparse_set.hpp and index_map.hpp. The registry
// names them for any element type, so they are unconstrained templates; their container_traits declare properties
// only for indexable keys.

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <type_traits>

namespace urd::repr
{

/// A map from indices to values, a slot per index below the largest key. Generations: clear starts a new one, O(1).
/// Stable: the slots never move. ThreadSafe (with Stable and Generations): update and read from threads at once.
template<typename K, typename V, bool Generations, bool ThreadSafe = false, bool Stable = ThreadSafe>
class IndexMap;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// dense map (a slot per index)
struct dense_map_decl
{
    static constexpr std::string_view id = "dense_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/index_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ order::sorted, uniqueness::unique, keys::dense, memory::retains, referencing::none });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>(), traits<role::order, trait::own_order>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_a>,
                                       cost_entry<operation::lookup, costs::one>,
                                       cost_entry<operation::ordered_lookup, costs::lin_keys>,
                                       cost_entry<operation::erase, costs::one>,
                                       cost_entry<operation::clear, costs::lin_keys>,
                                       cost_entry<operation::iterate, costs::lin_keys>>;
    static constexpr double words = 1.2;
    static constexpr std::string_view description = "dense map (a slot per index)";
    static constexpr std::string_view holds = "a slot per index";
};
template<typename K, typename V>
struct declaration_of<repr::IndexMap<K, V, false>>
{
    using declaration = dense_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

namespace detail
{

/// dense map over slots that never move (segments)
struct stable_dense_map_decl
{
    static constexpr std::string_view id = "stable_dense_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/index_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ order::sorted, uniqueness::unique, keys::dense, memory::retains, referencing::stable_refs });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>(), traits<role::order, trait::own_order>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_a>,
                                       cost_entry<operation::lookup, costs::one>,
                                       cost_entry<operation::ordered_lookup, costs::lin_keys>,
                                       cost_entry<operation::erase, costs::one>,
                                       cost_entry<operation::clear, costs::lin_keys>,
                                       cost_entry<operation::iterate, costs::lin_keys>>;
    static constexpr double words = 1.3;
    static constexpr std::string_view description = "dense map over slots that never move (segments)";
    static constexpr std::string_view holds = "a slot per index, in segments that never move";
};
template<typename K, typename V>
struct declaration_of<repr::IndexMap<K, V, false, false, true>>
{
    using declaration = stable_dense_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

namespace detail
{

/// generation map (a slot per index, O(1) clear by generation)
struct generation_map_decl
{
    static constexpr std::string_view id = "generation_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/index_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides =
        property_set::of({ order::sorted, uniqueness::unique, keys::dense, memory::retains, clearing::constant_clear, referencing::none, threading::none });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>(), traits<role::order, trait::own_order>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_a>,
                                       cost_entry<operation::lookup, costs::one>,
                                       cost_entry<operation::ordered_lookup, costs::lin_keys>,
                                       cost_entry<operation::erase, costs::one>,
                                       cost_entry<operation::clear, costs::one>,
                                       cost_entry<operation::iterate, costs::lin_keys>>;
    static constexpr double words = 1.6;
    static constexpr std::string_view description = "generation map (a slot per index, O(1) clear by generation)";
    static constexpr std::string_view holds = "a slot and a generation per index";
};
template<typename K, typename V>
struct declaration_of<repr::IndexMap<K, V, true>>
{
    using declaration = generation_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

namespace detail
{

/// generation map over slots that never move (segments)
struct stable_generation_map_decl
{
    static constexpr std::string_view id = "stable_generation_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/index_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of(
        { order::sorted, uniqueness::unique, keys::dense, memory::retains, clearing::constant_clear, referencing::stable_refs, threading::none });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>(), traits<role::order, trait::own_order>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_a>,
                                       cost_entry<operation::lookup, costs::one>,
                                       cost_entry<operation::ordered_lookup, costs::lin_keys>,
                                       cost_entry<operation::erase, costs::one>,
                                       cost_entry<operation::clear, costs::one>,
                                       cost_entry<operation::iterate, costs::lin_keys>>;
    static constexpr double words = 1.7;
    static constexpr std::string_view description = "generation map over slots that never move (segments)";
    static constexpr std::string_view holds = "a slot and a generation per index, in segments that never move";
};
template<typename K, typename V>
struct declaration_of<repr::IndexMap<K, V, true, false, true>>
{
    using declaration = stable_generation_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

namespace detail
{

/// generation map, thread-safe updates (a slot and a spin lock per index)
struct thread_safe_generation_map_decl
{
    static constexpr std::string_view id = "thread_safe_generation_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/index_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of(
        { order::sorted, uniqueness::unique, keys::dense, memory::retains, clearing::constant_clear, referencing::stable_refs, threading::concurrent });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>(), traits<role::order, trait::own_order>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_a>,
                                       cost_entry<operation::lookup, costs::one>,
                                       cost_entry<operation::ordered_lookup, costs::lin_keys>,
                                       cost_entry<operation::erase, costs::one>,
                                       cost_entry<operation::clear, costs::one>,
                                       cost_entry<operation::iterate, costs::lin_keys>>;
    static constexpr double words = 1.8;
    static constexpr std::string_view description = "generation map, thread-safe updates (a slot and a spin lock per index)";
    static constexpr std::string_view holds = "a slot, a generation and a lock byte per index, in segments";
};
template<typename K, typename V>
struct declaration_of<repr::IndexMap<K, V, true, true>>
{
    using declaration = thread_safe_generation_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

}  // namespace urd
