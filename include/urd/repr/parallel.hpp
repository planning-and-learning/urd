#pragma once

#include "urd/elements.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"

#include <cstddef>
#include <gtl/phmap.hpp>
#include <memory>
#include <mutex>
#include <utility>

// gtl's sharded hash tables: threads may insert into and look up in them at once, each shard behind its own mutex.
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

}  // namespace urd
