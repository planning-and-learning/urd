#pragma once

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <boost/unordered/unordered_flat_map_fwd.hpp>
#include <boost/unordered/unordered_flat_set_fwd.hpp>
#include <functional>

namespace urd
{

// Boost's open-addressing hash tables, with a natural hash on hashable elements. Elements live in one array of
// slots, so a rehash moves them (no stable_refs); erase and clear keep the slots. Declared here through Boost's
// forward headers; defined by urd/repr/values/unordered_flat_set.hpp and urd/repr/maps/unordered_flat_map.hpp.

}  // namespace urd
namespace urd::detail
{

/// boost::unordered_flat_map (open-addressing hash table)
struct unordered_flat_map_decl
{
    static constexpr std::string_view id = "unordered_flat_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/unordered_flat_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, keys::sparse, memory::retains });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::one_ae;
        static constexpr cost lookup = costs::one_e;
        static constexpr cost erase = costs::one_e;
        static constexpr cost clear = costs::lin_capacity;
        static constexpr cost iterate = costs::lin_capacity;
        static constexpr double words = 1.3;
    };
    static constexpr std::string_view description = "boost::unordered_flat_map (open-addressing hash table)";
    static constexpr std::string_view holds = "~n/0.875 slots + 1 byte each; retains";
};
template<typename K, typename V, typename H, typename E, typename A>
struct declaration_of<boost::unordered_flat_map<K, V, H, E, A>>
{
    using declaration = unordered_flat_map_decl;
    static constexpr property_set roles = map_roles<K, V>();
};

}  // namespace urd::detail
