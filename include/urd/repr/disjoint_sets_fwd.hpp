#pragma once

// A partition of indices into disjoint classes (union-find), declared with what the registry reads of it. Defined in
// urd/repr/disjoint_sets.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P disjoint_sets(const T& t)
{
    if (!t.indexable)
        return {};
    return property_set::of({ tag::partition, tag::dense, tag::shrinkable, tag::retains });
}


}  // namespace urd::detail::guarantees


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
    static constexpr auto headers = header_list<1> { "urd/repr/disjoint_sets.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::disjoint_sets(f.element);
        if (!held)
            return {};
        return offer_of(urd::kind::partition, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost lookup = costs::one_at;
        static constexpr cost merge = costs::one_at;
        static constexpr double words = 2;
    };
    static constexpr std::string_view description = "disjoint sets (union-find by size, path halving)";
    static constexpr std::string_view holds = "a parent and a size per element";
};
template<typename K>
struct declaration_of<repr::DisjointSets<K>>
{
    using declaration = disjoint_sets_decl;
    static constexpr request_features features = element_facts<K>();
};

}  // namespace urd::detail
