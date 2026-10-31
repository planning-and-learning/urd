#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"
#include "urd/repr/values/parallel_hash_set_fwd.hpp"

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

/// Update and read go through the table's lazy_emplace_l and if_contains, under the key's shard lock (the
/// values_by_key facet).
template<typename K, typename V>
using ParallelHashMap = gtl::parallel_flat_hash_map<K, V, Hash<K>, EqualTo<K>, std::allocator<std::pair<const K, V>>, shard_bits, std::mutex>;

template<typename K, typename V>
using ParallelNodeHashMap = gtl::parallel_node_hash_map<K, V, Hash<K>, EqualTo<K>, std::allocator<std::pair<const K, V>>, shard_bits, std::mutex>;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// parallel hash map (gtl: shards, each behind its mutex; update and read under it)
struct parallel_hash_map_decl
{
    static constexpr std::string_view id = "parallel_hash_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/parallel_hash_map.hpp" };
    static constexpr urd::kind kind = urd::kind::map;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, keys::sparse, threading::concurrent, referencing::none });
    static constexpr role_tags relies_on { .element = property_set::of({ hashability::can_hash }) };
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
    static constexpr std::string_view description = "parallel hash map (gtl: shards, each behind its mutex; update and read under it)";
    static constexpr std::string_view holds = "16 shards of open addressing, a lock each";
};
template<typename K, typename V>
struct declaration_of<repr::ParallelHashMap<K, V>>
{
    using declaration = parallel_hash_map_decl;
    static constexpr role_tags roles = map_roles<K, V>();
};

}  // namespace detail

namespace detail
{

/// parallel node hash map (gtl: shards behind mutexes, a node per entry that never moves)
struct parallel_node_hash_map_decl
{
    static constexpr std::string_view id = "parallel_node_hash_map";
    static constexpr auto headers = header_list<1> { "urd/repr/maps/parallel_hash_map.hpp" };
    static constexpr urd::kind kind = urd::kind::map;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, keys::sparse, threading::concurrent, referencing::stable_refs });
    static constexpr role_tags relies_on { .element = property_set::of({ hashability::can_hash }) };
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::one_ae;
        static constexpr cost lookup = costs::one_e;
        static constexpr cost erase = costs::one_e;
        static constexpr cost clear = costs::lin_capacity;
        static constexpr cost iterate = costs::lin_capacity;
        static constexpr double words = 3;
    };
    static constexpr std::string_view description = "parallel node hash map (gtl: shards behind mutexes, a node per entry that never moves)";
    static constexpr std::string_view holds = "16 shards, a node per entry; stable references";
};
template<typename K, typename V>
struct declaration_of<repr::ParallelNodeHashMap<K, V>>
{
    using declaration = parallel_node_hash_map_decl;
    static constexpr role_tags roles = map_roles<K, V>();
};

}  // namespace detail

}  // namespace urd
