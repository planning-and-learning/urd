#pragma once

// A map whose entries are kept in an order that may read the values, declared with what the registry reads of it.
// Defined in urd/repr/maps/entry_ordered.hpp.

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

namespace urd::repr
{

/// One vector of entries in Order (urd::EntryLess: by P(key, value), ties by key), found by key by a scan.
template<typename K, typename V, typename Order>
class FlatEntryMap;

}  // namespace urd::repr

namespace urd::detail
{

/// flat entry map (entries in an order that reads their values, found by key)
struct flat_entry_map_decl
{
    static constexpr std::string_view id = "flat_entry_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/entry_ordered.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ order::sorted, uniqueness::unique, position::positional, layout::contiguous, memory::retains });
    static constexpr property_set relies_on = relies(traits<role::order, trait::by_entry>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::lin_n>,
                                       cost_entry<operation::lookup, costs::lin_n>,
                                       cost_entry<operation::erase, costs::lin_n>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>>;
    static constexpr double words = 1.5;
    static constexpr std::string_view description = "flat entry map (entries in an order that reads their values, found by key)";
    static constexpr std::string_view holds = "n entries in one array; for few";
};
template<typename K, typename V, typename Order>
struct declaration_of<repr::FlatEntryMap<K, V, Order>>
{
    using declaration = flat_entry_map_decl;
    static constexpr property_set roles = map_roles<K, V, Order>();
};

}  // namespace urd::detail
