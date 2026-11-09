#pragma once

#include "urd/cost.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <functional>
#include <gtl/phmap_fwd_decl.hpp>

namespace urd
{

// gtl's open-addressing hash tables, with a natural hash on hashable elements. Elements live in one array of
// slots, so a rehash moves them (no stable_refs); erase and clear keep the slots. Declared here through gtl's
// forward header; defined by urd/repr/values/unordered_flat_set.hpp and urd/repr/maps/unordered_flat_map.hpp.

}  // namespace urd
namespace urd::detail
{

struct unordered_flat_set_decl
{
    static constexpr std::string_view id = "unordered_flat_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/unordered_flat_set.hpp" };
    using kind = set_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, keys::sparse, memory::retains });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_ae>,
                                       cost_entry<operation::lookup, costs::one_e>,
                                       cost_entry<operation::erase, costs::one_e>,
                                       cost_entry<operation::clear, costs::lin_capacity>,
                                       cost_entry<operation::iterate, costs::lin_capacity>>;
    static constexpr double words = 1.3;
    static constexpr quantity words_per = quantity::capacity;
    static constexpr std::string_view description = "gtl::flat_hash_set (open-addressing hash table)";
    static constexpr std::string_view holds = "~n/0.875 slots + 1 byte each; retains";
};
template<typename T, typename H, typename E, typename A>
struct declaration_of<gtl::flat_hash_set<T, H, E, A>>
{
    using declaration = unordered_flat_set_decl;
    static constexpr property_set roles = roles_of<urd::set<T>>();
};

}  // namespace urd::detail
