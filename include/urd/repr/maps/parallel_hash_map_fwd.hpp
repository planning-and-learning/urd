#pragma once

#include "urd/costs.hpp"
#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/repr/values/parallel_hash_set_fwd.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <cstddef>
#include <gtl/phmap_fwd_decl.hpp>
#include <memory>
#include <mutex>
#include <utility>

// gtl's sharded hash tables: threads may insert into and look up in them at once, each shard behind its own mutex.
// Declared here through gtl's forward header; defined by urd/repr/values/parallel_hash_set.hpp.
// ponytail: gtl's B-trees are left out: btree_map lacks insert_or_assign, and std::set and std::map serve sorted
// requests until a benchmark says otherwise.
namespace urd::repr
{

/// gtl's sharded tables, whose update and read go through lazy_emplace_l and if_contains, under the key's shard lock
/// (what the values_by_key facet calls); defined in urd/repr/maps/parallel_hash_map.hpp.
template<typename K, typename V>
class ParallelHashMap;
template<typename K, typename V>
class ParallelNodeHashMap;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

struct parallel_hash_map_decl
{
    static constexpr std::string_view id = "parallel_hash_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/parallel_hash_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, keys::sparse, threading::concurrent, referencing::none });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_ae>,
                                       cost_entry<operation::lookup, costs::one_e>,
                                       cost_entry<operation::erase, costs::one_e>,
                                       cost_entry<operation::clear, costs::lin_capacity>,
                                       cost_entry<operation::iterate, costs::lin_capacity>>;
    static constexpr double words = 1.3;
    static constexpr std::string_view description = "parallel hash map (gtl: shards, each behind its mutex; update and read under it)";
    static constexpr std::string_view holds = "16 shards of open addressing, a lock each";
};
template<typename K, typename V>
struct declaration_of<repr::ParallelHashMap<K, V>>
{
    using declaration = parallel_hash_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

namespace detail
{

struct parallel_node_hash_map_decl
{
    static constexpr std::string_view id = "parallel_node_hash_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/parallel_hash_map.hpp" };
    using kind = map_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, keys::sparse, threading::concurrent, referencing::stable_refs });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_ae>,
                                       cost_entry<operation::lookup, costs::one_e>,
                                       cost_entry<operation::erase, costs::one_e>,
                                       cost_entry<operation::clear, costs::lin_capacity>,
                                       cost_entry<operation::iterate, costs::lin_capacity>>;
    static constexpr double words = 3;
    static constexpr std::string_view description = "parallel node hash map (gtl: shards behind mutexes, a node per entry that never moves)";
    static constexpr std::string_view holds = "16 shards, a node per entry; stable references";
};
template<typename K, typename V>
struct declaration_of<repr::ParallelNodeHashMap<K, V>>
{
    using declaration = parallel_node_hash_map_decl;
    static constexpr property_set roles = roles_of<urd::map<K, V>>();
};

}  // namespace detail

}  // namespace urd
