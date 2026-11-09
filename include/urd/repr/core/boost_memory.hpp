#pragma once

// The memory of Boost's sorted vectors (boost::container::flat_*), which urd resolves to (urd/memory.hpp, memory_of).
// Declared on Boost's forward declarations, so this header costs no Boost container's compile.

#include "urd/memory.hpp"

#include <boost/container/container_fwd.hpp>
#include <cstddef>

namespace urd::detail
{

/// A sorted vector's capacity, and what its elements own.
template<typename Flat>
std::size_t flat_bytes(const Flat& held)
{ return (held.capacity() * sizeof(typename Flat::value_type)) + held_bytes(held); }

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
}  // namespace urd
