#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/registry/guarantees.hpp"

#include <cstddef>
#include <gtl/phmap_fwd_decl.hpp>
#include <memory>
#include <mutex>
#include <utility>

// gtl's sharded hash tables: threads may insert into and look up in them at once, each shard behind its own mutex.
// Declared here through gtl's forward header; defined by urd/repr/parallel_hash.hpp.
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






}  // namespace urd

namespace urd
{





}  // namespace urd

namespace urd
{

namespace detail
{

/// parallel hash set (gtl: shards, each behind its mutex)
struct parallel_hash_set_decl
{
    static constexpr std::string_view id = "parallel_hash_set";
    static constexpr auto headers = header_list<1> { "urd/repr/parallel_hash.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::parallel_hash_set(f.element, false);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
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
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

namespace detail
{

/// parallel hash map (gtl: shards, each behind its mutex; update and read under it)
struct parallel_hash_map_decl
{
    static constexpr std::string_view id = "parallel_hash_map";
    static constexpr auto headers = header_list<1> { "urd/repr/parallel_hash.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::parallel_hash_map(f.key, false);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
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
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// parallel node hash set (gtl: shards behind mutexes, a node per element that never moves)
struct parallel_node_hash_set_decl
{
    static constexpr std::string_view id = "parallel_node_hash_set";
    static constexpr auto headers = header_list<1> { "urd/repr/parallel_hash.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::parallel_hash_set(f.element, true);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
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
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

namespace detail
{

/// parallel node hash map (gtl: shards behind mutexes, a node per entry that never moves)
struct parallel_node_hash_map_decl
{
    static constexpr std::string_view id = "parallel_node_hash_map";
    static constexpr auto headers = header_list<1> { "urd/repr/parallel_hash.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::parallel_hash_map(f.key, true);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
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
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

}  // namespace urd
