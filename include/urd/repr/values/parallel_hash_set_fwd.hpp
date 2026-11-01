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

/// How many shards a parallel table has: 2^shard_bits, each with its own mutex.
inline constexpr std::size_t shard_bits = 4;

template<typename T>
using ParallelHashSet = gtl::parallel_flat_hash_set<T, Hash<T>, EqualTo<T>, std::allocator<T>, shard_bits, std::mutex>;

/// The same tables with a node per element: a rehash moves the nodes' pointers, not the elements, so references
/// hold while threads insert.
template<typename T>
using ParallelNodeHashSet = gtl::parallel_node_hash_set<T, Hash<T>, EqualTo<T>, std::allocator<T>, shard_bits, std::mutex>;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// parallel hash set (gtl: shards, each behind its mutex)
struct parallel_hash_set_decl
{
    static constexpr std::string_view id = "parallel_hash_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/parallel_hash_set.hpp" };
    using kind = set_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, keys::sparse, threading::concurrent, referencing::none });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::can_hash>());
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
    static constexpr std::string_view description = "parallel hash set (gtl: shards, each behind its mutex)";
    static constexpr std::string_view holds = "16 shards of open addressing, a lock each";
};
template<typename T>
struct declaration_of<repr::ParallelHashSet<T>>
{
    using declaration = parallel_hash_set_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// parallel node hash set (gtl: shards behind mutexes, a node per element that never moves)
struct parallel_node_hash_set_decl
{
    static constexpr std::string_view id = "parallel_node_hash_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/parallel_hash_set.hpp" };
    using kind = set_kind;
    static constexpr property_set provides = property_set::of({ uniqueness::unique, keys::sparse, threading::concurrent, referencing::stable_refs });
    static constexpr role_tags relies_on = relies(traits<role::element, trait::can_hash>());
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
    static constexpr std::string_view description = "parallel node hash set (gtl: shards behind mutexes, a node per element that never moves)";
    static constexpr std::string_view holds = "16 shards, a node per element; stable references";
};
template<typename T>
struct declaration_of<repr::ParallelNodeHashSet<T>>
{
    using declaration = parallel_node_hash_set_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

}  // namespace urd
