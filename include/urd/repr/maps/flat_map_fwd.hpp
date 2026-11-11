#pragma once

#include "urd/cost.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <boost/container/container_fwd.hpp>
#include <functional>

namespace urd
{

// Boost's flat sets are sorted, as std::set is, by a natural order on orderable elements.
// Their vector keeps its capacity when elements are erased. Declared here through Boost's forward header; defined by
// urd/repr/values/flat_set.hpp and urd/repr/maps/flat_map.hpp.

// Their maps likewise, sorted by key. Their iterators would let a key be overwritten; a Store hands out only
// const ones.

// A multimap keeps equal keys' values in insertion order, so equal contents inserted in another order differ:
// it is not canonical.

}  // namespace urd
namespace urd
{

namespace detail
{

struct flat_map_decl
{
    static constexpr std::string_view id = "flat_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/flat_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ order::sorted,
                                                                uniqueness::unique,
                                                                position::positional,
                                                                keys::sparse,
                                                                layout::contiguous,
                                                                representation::canonical,
                                                                memory::retains,
                                                                clearing::proportional_clear,
                                                                room::reservable });
    static constexpr property_set relies_on = relies(traits<role::order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::lin_n>,
                                       cost_entry<operation::lookup, costs::log_n>,
                                       cost_entry<operation::ordered_lookup, costs::log_n>,
                                       cost_entry<operation::erase, costs::lin_n>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>>;
    static constexpr double words = 1.5;
    static constexpr std::string_view description = "boost::container::flat_map (sorted vector)";
    static constexpr std::string_view holds = "n entries in one array; retains";
};
template<typename K, typename V, typename C, typename A>
struct declaration_of<boost::container::flat_map<K, V, C, A>>
{
    using declaration = flat_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>, C>();
};

}  // namespace detail

namespace detail
{

struct flat_multimap_decl : flat_map_decl
{
    static constexpr std::string_view id = "flat_multimap";
    static constexpr property_set provides = property_set::of(
        { order::sorted, multiplicity::multi, position::positional, layout::contiguous, memory::retains, clearing::proportional_clear, room::reservable });
    static constexpr std::string_view description = "boost::container::flat_multimap (sorted vector)";
};
template<typename K, typename V, typename C, typename A>
struct declaration_of<boost::container::flat_multimap<K, V, C, A>>
{
    using declaration = flat_multimap_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>, C>();
};

}  // namespace detail

}  // namespace urd
