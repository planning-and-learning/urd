#pragma once

// The memory of gtl's sharded hash tables (urd/memory.hpp, memory_of), which urd's parallel sets and maps are: counted
// by gtl itself, so its layout is gtl's.

#include "urd/memory.hpp"

#include <cstddef>
#include <gtl/phmap.hpp>
#include <type_traits>

namespace urd::detail
{

/// A gtl sharded table's memory: each shard's buffer of control bytes and slots, with a node per element where nodes
/// are allocated (gtl's own count, AllocatedByteSize), and what the elements own.
template<typename Table>
std::size_t sharded_table_bytes(const Table& held)
{
    using Shard = std::remove_cvref_t<decltype(held.get_inner(0).set_)>;
    std::size_t result = 0;
    for (std::size_t each = 0; each < Table::subcnt(); ++each)
        result += gtl::priv::hashtable_debug_internal::HashtableDebugAccess<Shard>::AllocatedByteSize(held.get_inner(each).set_);
    return result + held_bytes(held);
}

}  // namespace urd::detail

namespace urd
{

/// The sharded sets: what urd's parallel sets are, and the table of a thread-safe indexed set.
template<typename T, typename H, typename E, typename A, std::size_t N, typename M, typename C>
struct memory_of<gtl::parallel_flat_hash_set<T, H, E, A, N, M, C>>
{
    static std::size_t bytes(const gtl::parallel_flat_hash_set<T, H, E, A, N, M, C>& held) { return detail::sharded_table_bytes(held); }
};
/// gtl's single table: a control byte and a slot per bucket, and what the elements own.
template<typename T, typename H, typename E, typename A>
struct memory_of<gtl::flat_hash_set<T, H, E, A>>
{
    static std::size_t bytes(const gtl::flat_hash_set<T, H, E, A>& held)
    { return (held.capacity() == 0 ? 0 : held.capacity() * (sizeof(T) + 1)) + detail::held_bytes(held); }
};
template<typename T, typename H, typename E, typename A, std::size_t N, typename M, typename C>
struct memory_of<gtl::parallel_node_hash_set<T, H, E, A, N, M, C>>
{
    static std::size_t bytes(const gtl::parallel_node_hash_set<T, H, E, A, N, M, C>& held) { return detail::sharded_table_bytes(held); }
};

}  // namespace urd
