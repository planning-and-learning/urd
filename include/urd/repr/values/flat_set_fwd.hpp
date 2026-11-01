#pragma once

#include "urd/costs.hpp"
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

/// boost::container::flat_set (sorted vector)
struct flat_set_decl
{
    static constexpr std::string_view id = "flat_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/flat_set.hpp" };
    using kind = set_kind;
    static constexpr property_set provides = property_set::of({ order::sorted,
                                                                uniqueness::unique,
                                                                position::positional,
                                                                keys::sparse,
                                                                layout::contiguous,
                                                                representation::canonical,
                                                                memory::retains,
                                                                clearing::proportional_clear });
    static constexpr property_set relies_on = relies(traits<role::order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::lin_n;
        static constexpr cost lookup = costs::log_n;
        static constexpr cost ordered_lookup = costs::log_n;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr cost access = costs::one;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "boost::container::flat_set (sorted vector)";
    static constexpr std::string_view holds = "n elements in one array; retains";
};
template<typename T, typename C, typename A>
struct declaration_of<boost::container::flat_set<T, C, A>>
{
    using declaration = flat_set_decl;
    static constexpr property_set roles = element_roles<T, C>();
};

}  // namespace detail

namespace detail
{

/// boost::container::flat_multiset (sorted vector)
struct flat_multiset_decl
{
    static constexpr std::string_view id = "flat_multiset";
    static constexpr auto headers = header_list<1> { "urd/repr/values/flat_set.hpp" };
    using kind = multiset_kind;
    static constexpr property_set provides =
        property_set::of({ order::sorted, position::positional, layout::contiguous, representation::canonical, memory::retains, clearing::proportional_clear });
    static constexpr property_set relies_on = relies(traits<role::order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::lin_n;
        static constexpr cost lookup = costs::log_n;
        static constexpr cost ordered_lookup = costs::log_n;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr cost access = costs::one;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "boost::container::flat_multiset (sorted vector)";
    static constexpr std::string_view holds = "n elements in one array; retains";
};
template<typename T, typename C, typename A>
struct declaration_of<boost::container::flat_multiset<T, C, A>>
{
    using declaration = flat_multiset_decl;
    static constexpr property_set roles = element_roles<T, C>();
};

}  // namespace detail

}  // namespace urd
