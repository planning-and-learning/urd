#pragma once

// A map in insertion order over an insertion-ordered set of keys, declared with what the registry reads of it.
// Defined in urd/repr/maps/ordered_map.hpp; its keys (an indexed set or a sparse set) in theirs.

#include "urd/cost.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/repr/maps/dense_map_fwd.hpp"
#include "urd/repr/values/dense_set_fwd.hpp"
#include "urd/repr/values/indexed_fwd.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <type_traits>

namespace urd::repr
{

/// A map in insertion order: its keys in Keys, an insertion-ordered set whose positions index the values, and the
/// values in a vector beside them.
template<typename Keys, typename V>
class OrderedMap;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

struct ordered_map_decl
{
    static constexpr std::string_view id = "ordered_map";
    static constexpr auto headers = with_header(indexed_set_decl::headers, "urd/repr/maps/ordered_map.hpp");
    using kind = map_kind;
    static constexpr property_set provides =
        property_set::of({ order::insertion_ordered, uniqueness::unique, position::none, layout::none, memory::retains, room::reservable });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = costs_over<indexed_set_decl, cost_entry<operation::erase, costs::lin_n>, cost_entry<operation::iterate, costs::lin_n>>;
    static constexpr double words = indexed_set_decl::words + 1;
    static constexpr std::string_view description = "ordered map (keys in an insertion-ordered set, values beside them)";
    static constexpr std::string_view holds = "its key set + n values in order";
};
template<typename K, typename V>
struct declaration_of<repr::OrderedMap<repr::IndexedSet<K>, V>>
{
    using declaration = ordered_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

namespace detail
{

struct dense_ordered_map_decl
{
    static constexpr std::string_view id = "dense_ordered_map";
    static constexpr auto headers = with_header(ordered_sparse_set_decl::headers, "urd/repr/maps/ordered_map.hpp");
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ order::insertion_ordered,
                                                                uniqueness::unique,
                                                                keys::dense,
                                                                position::none,
                                                                layout::none,
                                                                memory::retains,
                                                                clearing::constant_clear,
                                                                room::reservable });
    static constexpr property_set relies_on =
        relies(traits<role::element, trait::an_index, trait::destroys_trivially>(), traits<role::mapped, trait::destroys_trivially>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = costs_over<ordered_sparse_set_decl, cost_entry<operation::erase, costs::lin_n>, cost_entry<operation::iterate, costs::lin_n>>;
    static constexpr double words = ordered_sparse_set_decl::words + 1;
    static constexpr std::string_view description = "dense ordered map (indices in an insertion-ordered sparse set, values beside them)";
    static constexpr std::string_view holds = "its sparse set + n values in order";
};
template<typename K, typename V>
struct declaration_of<repr::OrderedMap<repr::SparseSet<K, true>, V>>
{
    using declaration = dense_ordered_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

}  // namespace urd
