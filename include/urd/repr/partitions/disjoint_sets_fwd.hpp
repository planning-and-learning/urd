#pragma once

// A partition of indices into disjoint classes (union-find), declared with what the registry reads of it. Defined in
// urd/repr/partitions/disjoint_sets.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

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

/// disjoint sets (union-find by size, path halving)
struct disjoint_sets_decl
{
    static constexpr std::string_view id = "disjoint_sets";
    static constexpr auto headers = header_list<1> { "urd/repr/partitions/disjoint_sets.hpp" };
    static constexpr urd::kind kind = urd::kind::partition;
    static constexpr property_set provides = property_set::of({ grouping::partition, memory::retains });
    static constexpr role_tags relies_on { .element = property_set::of({ indexing::an_index }) };
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost lookup = costs::one_a;
        static constexpr cost merge = costs::one_a;
        static constexpr double words = 2;
    };
    static constexpr std::string_view description = "disjoint sets (union-find by size, path halving)";
    static constexpr std::string_view holds = "a parent and a size per element";
};
template<typename K>
struct declaration_of<repr::DisjointSets<K>>
{
    using declaration = disjoint_sets_decl;
    static constexpr role_tags roles = element_roles<K>();
};

}  // namespace urd::detail
