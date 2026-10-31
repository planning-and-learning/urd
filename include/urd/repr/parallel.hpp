#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"

#include <cstddef>
#include <gtl/phmap_fwd_decl.hpp>
#include <memory>
#include <mutex>
#include <utility>

// gtl's sharded hash tables: threads may insert into and look up in them at once, each shard behind its own mutex.
// Declared here through gtl's forward header; defined by urd/use/parallel.hpp.
// ponytail: gtl's B-trees are left out: btree_map lacks insert_or_assign, and std::set and std::map serve sorted
// requests until a benchmark says otherwise.

namespace urd::repr
{

/// How many shards a parallel table has: 2^shard_bits, each with its own mutex.
inline constexpr std::size_t shard_bits = 4;

template<typename T>
using ParallelHashSet = gtl::parallel_flat_hash_set<T, Hash<T>, EqualTo<T>, std::allocator<T>, shard_bits, std::mutex>;

/// Update and read go through the table's lazy_emplace_l and if_contains, under the key's shard lock (the
/// values_by_key facet).
template<typename K, typename V>
using ParallelHashMap = gtl::parallel_flat_hash_map<K, V, Hash<K>, EqualTo<K>, std::allocator<std::pair<const K, V>>, shard_bits, std::mutex>;

/// The same tables with a node per element: a rehash moves the nodes' pointers, not the elements, so references
/// hold while threads insert.
template<typename T>
using ParallelNodeHashSet = gtl::parallel_node_hash_set<T, Hash<T>, EqualTo<T>, std::allocator<T>, shard_bits, std::mutex>;
template<typename K, typename V>
using ParallelNodeHashMap = gtl::parallel_node_hash_map<K, V, Hash<K>, EqualTo<K>, std::allocator<std::pair<const K, V>>, shard_bits, std::mutex>;

}  // namespace urd::repr

namespace urd
{

/// Threads insert and look up at once, each shard behind its mutex; iteration, erase and clear need no other operation
/// running. A rehash moves the elements of its shard.
template<typename T, typename H, typename E, typename A, std::size_t N, typename Aux>
    requires natural_hash<H, E, T>
struct container_traits<gtl::parallel_flat_hash_set<T, H, E, A, N, std::mutex, Aux>> : properties_of<unique, sparse, shrinkable, concurrent>
{
};

/// As the set; update and read too.
template<typename K, typename V, typename H, typename E, typename A, std::size_t N, typename Aux>
    requires natural_hash<H, E, K>
struct container_traits<gtl::parallel_flat_hash_map<K, V, H, E, A, N, std::mutex, Aux>> : properties_of<unique, functional, sparse, shrinkable, concurrent>
{
};

/// A node per element: references hold through inserts from threads at once.
template<typename T, typename H, typename E, typename A, std::size_t N, typename Aux>
    requires natural_hash<H, E, T>
struct container_traits<gtl::parallel_node_hash_set<T, H, E, A, N, std::mutex, Aux>> : properties_of<unique, sparse, stable_refs, shrinkable, concurrent>
{
};

template<typename K, typename V, typename H, typename E, typename A, std::size_t N, typename Aux>
    requires natural_hash<H, E, K>
struct container_traits<gtl::parallel_node_hash_map<K, V, H, E, A, N, std::mutex, Aux>> :
    properties_of<unique, functional, sparse, stable_refs, shrinkable, concurrent>
{
};

template<typename T, typename H, typename E, typename A, std::size_t N, typename M, typename Aux>
inline constexpr auto defined_in<gtl::parallel_flat_hash_set<T, H, E, A, N, M, Aux>> = detail::fixed_string("urd/use/parallel.hpp");
template<typename K, typename V, typename H, typename E, typename A, std::size_t N, typename M, typename Aux>
inline constexpr auto defined_in<gtl::parallel_flat_hash_map<K, V, H, E, A, N, M, Aux>> = detail::fixed_string("urd/use/parallel.hpp");
template<typename T, typename H, typename E, typename A, std::size_t N, typename M, typename Aux>
inline constexpr auto defined_in<gtl::parallel_node_hash_set<T, H, E, A, N, M, Aux>> = detail::fixed_string("urd/use/parallel.hpp");
template<typename K, typename V, typename H, typename E, typename A, std::size_t N, typename M, typename Aux>
inline constexpr auto defined_in<gtl::parallel_node_hash_map<K, V, H, E, A, N, M, Aux>> = detail::fixed_string("urd/use/parallel.hpp");

}  // namespace urd

namespace urd
{

/// parallel_hash_set.
template<typename T>
struct container_costs<repr::ParallelHashSet<T>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::one_e;
    static constexpr cost clear = costs::lin_capacity;
    static constexpr cost iterate = costs::lin_capacity;
    static constexpr double words = 1.3;
};

/// parallel_hash_map.
template<typename K, typename V>
struct container_costs<repr::ParallelHashMap<K, V>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::one_e;
    static constexpr cost clear = costs::lin_capacity;
    static constexpr cost iterate = costs::lin_capacity;
    static constexpr double words = 1.3;
};

/// parallel_node_hash_set.
template<typename T>
struct container_costs<repr::ParallelNodeHashSet<T>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::one_e;
    static constexpr cost clear = costs::lin_capacity;
    static constexpr cost iterate = costs::lin_capacity;
    static constexpr double words = 3;
};

/// parallel_node_hash_map.
template<typename K, typename V>
struct container_costs<repr::ParallelNodeHashMap<K, V>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::one_e;
    static constexpr cost clear = costs::lin_capacity;
    static constexpr cost iterate = costs::lin_capacity;
    static constexpr double words = 3;
};

}  // namespace urd

namespace urd
{

template<typename T>
struct container_synopsis<repr::ParallelHashSet<T>>
{
    static constexpr std::string_view id = "parallel_hash_set";
    static constexpr std::string_view description = "parallel hash set (gtl: shards, each behind its mutex)";
    static constexpr std::string_view holds = "16 shards of open addressing, a lock each";
};

template<typename K, typename V>
struct container_synopsis<repr::ParallelHashMap<K, V>>
{
    static constexpr std::string_view id = "parallel_hash_map";
    static constexpr std::string_view description = "parallel hash map (gtl: shards, each behind its mutex; update and read under it)";
    static constexpr std::string_view holds = "16 shards of open addressing, a lock each";
};

template<typename T>
struct container_synopsis<repr::ParallelNodeHashSet<T>>
{
    static constexpr std::string_view id = "parallel_node_hash_set";
    static constexpr std::string_view description = "parallel node hash set (gtl: shards behind mutexes, a node per element that never moves)";
    static constexpr std::string_view holds = "16 shards, a node per element; stable references";
};

template<typename K, typename V>
struct container_synopsis<repr::ParallelNodeHashMap<K, V>>
{
    static constexpr std::string_view id = "parallel_node_hash_map";
    static constexpr std::string_view description = "parallel node hash map (gtl: shards behind mutexes, a node per entry that never moves)";
    static constexpr std::string_view holds = "16 shards, a node per entry; stable references";
};

}  // namespace urd
