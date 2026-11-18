#pragma once

#include "urd/cost.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <cstddef>

namespace urd::repr
{
template<typename T, typename Compare, std::size_t Block>
class SortedBlockSet;
}  // namespace urd::repr

namespace urd::detail
{

/// A sorted sequence of unique values in blocks (urd/repr/values/sorted_block_set.hpp): what a sorted set that changes
/// resolves to (and a sorted multiset that states unique), inserting and erasing in O(log n), its elements by position.
/// A frozen or small one lies in one array instead (a flat set): its blocks come later there.
struct sorted_block_set_decl
{
    static constexpr std::string_view id = "sorted_block_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/sorted_block_set.hpp" };
    using kind = multiset_kind;
    static constexpr property_set provides = property_set::of({ order::sorted,
                                                                uniqueness::unique,
                                                                position::positional,
                                                                keys::sparse,
                                                                representation::canonical,
                                                                layout::blocks,
                                                                memory::retains,
                                                                clearing::proportional_clear });
    static constexpr property_set relies_on = relies(stated({ uniqueness::unique }), traits<role::order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::log_n>,
                                       cost_entry<operation::lookup, costs::log_n>,
                                       cost_entry<operation::ordered_lookup, costs::log_n>,
                                       cost_entry<operation::erase, costs::log_n>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>,
                                       cost_entry<operation::access, costs::log_n>>;
    static constexpr double words = 1.5;
    static constexpr std::string_view description = "SortedBlockSet (sorted blocks, positions by a Fenwick tree)";
    static constexpr std::string_view holds = "n elements in blocks of at most 1024; retains";
};

template<typename T, typename C, std::size_t B>
struct declaration_of<repr::SortedBlockSet<T, C, B>>
{
    using declaration = sorted_block_set_decl;
    static constexpr property_set roles = roles_of<urd::multiset<T>, C>();
};

}  // namespace urd::detail
