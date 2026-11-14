#pragma once

// The memory of the standard library's node containers and deque (urd/memory.hpp, memory_of): what urd resolves to
// (std::map, std::unordered_set, …) and builds on (Recycling, Scoped, Queue). Their nodes are the library's own, so
// their sizes are libstdc++'s layout: a tree node's header is a color and three pointers, a hash node's a next pointer
// and, where the library caches it, the hash; a deque's blocks are 512 bytes. The benchmarks check these against what
// is allocated (benchmarks/tools/record.py).

#include "urd/memory.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace urd::detail
{

/// The bytes of a node holding a V after a header of `header` bytes.
template<typename V>
constexpr std::size_t node_bytes(std::size_t header)
{
    constexpr auto align = std::max(alignof(V), alignof(void*));
    const auto value_at = (header + alignof(V) - 1) / alignof(V) * alignof(V);
    return (value_at + sizeof(V) + align - 1) / align * align;
}

/// A red-black tree's nodes: color and parent, left and right pointers, then the value.
template<typename Tree>
std::size_t tree_bytes(const Tree& held)
{ return (held.size() * node_bytes<typename Tree::value_type>(4 * sizeof(void*))) + held_bytes(held); }

/// A hash table's nodes (a next pointer, the value, and the hash where cached) and its buckets (one pointer each; a
/// single bucket is held inside the table).
template<typename Table>
std::size_t hash_table_bytes(const Table& held)
{
    using V = typename Table::value_type;
#if defined(__GLIBCXX__)
    constexpr bool cached = std::__cache_default<typename Table::key_type, typename Table::hasher>::value;
#else
    constexpr bool cached = true;  // ponytail: libstdc++'s layout; another library's is checked by the benchmarks
#endif
    const auto node = node_bytes<V>(sizeof(void*)) + (cached ? sizeof(std::size_t) : 0);
    const auto buckets = held.bucket_count() > 1 ? held.bucket_count() * sizeof(void*) : 0;
    return (held.size() * node) + buckets + held_bytes(held);
}

/// The bytes of the node a node handle holds: the library's node type, as its node allocator allocates it.
template<typename Handle>
struct kept_node;
#if defined(__GLIBCXX__)
template<typename K, typename V, typename NodeAllocator>
struct kept_node<std::_Node_handle<K, V, NodeAllocator>>
{
    static constexpr std::size_t bytes = sizeof(typename std::allocator_traits<NodeAllocator>::value_type);
};
#endif

}  // namespace urd::detail

namespace urd
{

template<typename K, typename V, typename C, typename A>
struct memory_of<std::map<K, V, C, A>>
{
    static std::size_t bytes(const std::map<K, V, C, A>& held) { return detail::tree_bytes(held); }
};
template<typename K, typename V, typename C, typename A>
struct memory_of<std::multimap<K, V, C, A>>
{
    static std::size_t bytes(const std::multimap<K, V, C, A>& held) { return detail::tree_bytes(held); }
};
template<typename T, typename C, typename A>
struct memory_of<std::set<T, C, A>>
{
    static std::size_t bytes(const std::set<T, C, A>& held) { return detail::tree_bytes(held); }
};
template<typename T, typename C, typename A>
struct memory_of<std::multiset<T, C, A>>
{
    static std::size_t bytes(const std::multiset<T, C, A>& held) { return detail::tree_bytes(held); }
};
template<typename K, typename V, typename H, typename E, typename A>
struct memory_of<std::unordered_map<K, V, H, E, A>>
{
    static std::size_t bytes(const std::unordered_map<K, V, H, E, A>& held) { return detail::hash_table_bytes(held); }
};
template<typename K, typename V, typename H, typename E, typename A>
struct memory_of<std::unordered_multimap<K, V, H, E, A>>
{
    static std::size_t bytes(const std::unordered_multimap<K, V, H, E, A>& held) { return detail::hash_table_bytes(held); }
};
template<typename T, typename H, typename E, typename A>
struct memory_of<std::unordered_set<T, H, E, A>>
{
    static std::size_t bytes(const std::unordered_set<T, H, E, A>& held) { return detail::hash_table_bytes(held); }
};

/// A deque's blocks of 512 bytes (one element each where larger), one past those in use, and its table of blocks.
template<typename T, typename A>
struct memory_of<std::deque<T, A>>
{
    static std::size_t bytes(const std::deque<T, A>& held)
    {
        constexpr std::size_t block_bytes = 512, least_table = 8;
        constexpr std::size_t block = sizeof(T) < block_bytes ? block_bytes / sizeof(T) * sizeof(T) : sizeof(T);
        constexpr std::size_t per_block = block / sizeof(T);
        const auto blocks = (held.size() / per_block) + 1;
        // ponytail: the block table's size is the library's; at least 8, two more than the blocks it points to
        return (blocks * block) + (std::max(least_table, blocks + 2) * sizeof(void*)) + detail::held_bytes(held);
    }
};

}  // namespace urd
