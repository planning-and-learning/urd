#pragma once

// A partition of indices into disjoint classes (union-find), declared with what the registry reads of it. Defined in
// urd/repr/partitions/disjoint_sets.hpp.

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

namespace urd::repr
{

/// A partition of the indices 0..n-1 into disjoint classes, each named by one of its elements. The registry names it
/// for any key type, so it is an unconstrained template; its container_traits declare properties only for
/// indexable keys.
template<typename K>
class DisjointSets;

}  // namespace urd::repr

namespace urd::detail
{

struct disjoint_sets_decl
{
    static constexpr std::string_view id = "disjoint_sets";
    static constexpr auto headers = header_list<1> { "urd/repr/partitions/disjoint_sets.hpp" };
    using kind = partition_kind;
    static constexpr property_set provides = property_set::of({ grouping::partition, memory::retains });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs =
        cost_table<cost_entry<operation::insert, costs::one_a>, cost_entry<operation::lookup, costs::one_a>, cost_entry<operation::merge, costs::one_a>>;
    static constexpr double words = 2;
    static constexpr std::string_view description = "disjoint sets (union-find by size, path halving)";
    static constexpr std::string_view holds = "a parent and a size per element";
};
template<typename K>
struct declaration_of<repr::DisjointSets<K>>
{
    using declaration = disjoint_sets_decl;
    static constexpr property_set roles = roles_of<urd::partition<K>>();
};

}  // namespace urd::detail
