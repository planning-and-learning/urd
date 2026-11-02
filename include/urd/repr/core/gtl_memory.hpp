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
