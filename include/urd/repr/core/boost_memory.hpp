#pragma once

// The memory of Boost's containers that urd resolves to or builds on (urd/memory.hpp, memory_of): the sorted vectors
// (boost::container::flat_*) and the open-addressing tables (boost::unordered_flat_*, concurrent_flat_set). Declared
// on Boost's forward declarations, so this header costs no Boost container's compile; the tables' layout is
// Boost.Unordered's: one buffer of slots and their groups of 15 (16 bytes each, one past them), and for the concurrent
// table a lock per group.

#include "urd/memory.hpp"

#include <boost/container/container_fwd.hpp>
#include <boost/unordered/concurrent_flat_set_fwd.hpp>
#include <boost/unordered/unordered_flat_map_fwd.hpp>
#include <boost/unordered/unordered_flat_set_fwd.hpp>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <type_traits>

namespace urd::detail
{

/// A sorted vector's capacity, and what its elements own.
template<typename Flat>
std::size_t flat_bytes(const Flat& held)
{ return (held.capacity() * sizeof(typename Flat::value_type)) + held_bytes(held); }

/// An open-addressing table's buffer: its slots (bucket_count(), groups of 15 less the sentinel) and their groups, as
/// one allocation in whole slots; and what its elements own.
template<typename Table>
std::size_t open_table_bytes(const Table& held, std::size_t per_group = 0)
{
    using V = typename Table::value_type;
    constexpr std::size_t group = 16, per = 15;
    const auto slots = held.bucket_count();
    if (slots == 0)
        return 0;
    const auto groups = (slots + 1) / per;
    const auto buffer = (sizeof(V) * slots) + (group * (groups + 1)) - 1;
    auto result = ((buffer + sizeof(V) - 1) / sizeof(V) * sizeof(V)) + (groups * per_group);
    if constexpr (std::ranges::range<Table>)
        result += held_bytes(held);
    else
        static_assert(std::is_trivially_copyable_v<V>, "urd::memory_of: a concurrent table of elements that own memory");
    return result;
}

}  // namespace urd::detail

namespace urd
{

template<typename K, typename V, typename C, typename A>
struct memory_of<boost::container::flat_map<K, V, C, A>>
{
    static std::size_t bytes(const boost::container::flat_map<K, V, C, A>& held) { return detail::flat_bytes(held); }
};
template<typename K, typename V, typename C, typename A>
struct memory_of<boost::container::flat_multimap<K, V, C, A>>
{
    static std::size_t bytes(const boost::container::flat_multimap<K, V, C, A>& held) { return detail::flat_bytes(held); }
};
template<typename T, typename C, typename A>
struct memory_of<boost::container::flat_set<T, C, A>>
{
    static std::size_t bytes(const boost::container::flat_set<T, C, A>& held) { return detail::flat_bytes(held); }
};
template<typename T, typename C, typename A>
struct memory_of<boost::container::flat_multiset<T, C, A>>
{
    static std::size_t bytes(const boost::container::flat_multiset<T, C, A>& held) { return detail::flat_bytes(held); }
};
template<typename K, typename V, typename H, typename E, typename A>
struct memory_of<boost::unordered_flat_map<K, V, H, E, A>>
{
    static std::size_t bytes(const boost::unordered_flat_map<K, V, H, E, A>& held) { return detail::open_table_bytes(held); }
};
template<typename T, typename H, typename E, typename A>
struct memory_of<boost::unordered_flat_set<T, H, E, A>>
{
    static std::size_t bytes(const boost::unordered_flat_set<T, H, E, A>& held) { return detail::open_table_bytes(held); }
};
/// The concurrent table holds a lock and an insert counter per group besides.
template<typename T, typename H, typename E, typename A>
struct memory_of<boost::concurrent_flat_set<T, H, E, A>>
{
    static std::size_t bytes(const boost::concurrent_flat_set<T, H, E, A>& held) { return detail::open_table_bytes(held, 2 * sizeof(std::uint32_t)); }
};

}  // namespace urd
