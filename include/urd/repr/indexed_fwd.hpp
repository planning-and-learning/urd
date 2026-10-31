#pragma once

// A set kept in insertion order whose positions are its elements' indices (an interner), declared with its defaults
// and what the registry reads of it. Defined in urd/repr/indexed.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <cstdint>

namespace urd::repr
{

/// The elements in a vector, and a hash table of their positions. Stable: the elements lie in segments that never
/// move. ThreadSafe (with Stable): threads insert and look up at once. The registry names it for any element type,
/// so it is an unconstrained template; its container_traits declare properties only for hashable elements.
template<typename T, typename Index = std::uint32_t, bool ThreadSafe = false, bool Stable = ThreadSafe>
class IndexedSet;

/// An IndexedSet whose elements never move: an interner that hands out references.
template<typename T, typename Index = std::uint32_t>
using StableIndexedSet = IndexedSet<T, Index, false, true>;
/// An IndexedSet that threads may insert into at once.
template<typename T, typename Index = std::uint32_t>
using ConcurrentIndexedSet = IndexedSet<T, Index, true, true>;

/// The table a pool interns its rows or nodes in: thread-safe where ThreadSafe.
template<typename T, bool ThreadSafe>
using InterningTable = IndexedSet<T, std::uint32_t, ThreadSafe>;

}  // namespace urd::repr

namespace urd
{

/// In insertion order, in one array; erase and clear keep the storage of both the array and the table.
template<hashable T, typename Index>
struct container_traits<repr::IndexedSet<T, Index>> : properties_of<insertion_ordered, unique, positional, contiguous, shrinkable, retains>
{
};

/// In insertion order, never moved, only ever added to; clear keeps the segments.
template<hashable T, typename Index>
struct container_traits<repr::StableIndexedSet<T, Index>> : properties_of<insertion_ordered, unique, positional, stable_refs, growable, retains>
{
};

/// The same, by threads at once.
template<hashable T, typename Index>
struct container_traits<repr::ConcurrentIndexedSet<T, Index>> : properties_of<insertion_ordered, unique, positional, stable_refs, growable, retains, concurrent>
{
};

template<typename T, typename Index, bool ThreadSafe, bool Stable>
inline constexpr auto defined_in<repr::IndexedSet<T, Index, ThreadSafe, Stable>> = detail::fixed_string("urd/repr/indexed.hpp");

}  // namespace urd

namespace urd
{

/// indexed_set.
template<typename T>
struct container_costs<repr::IndexedSet<T, std::uint32_t>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::lin_n;
    static constexpr cost clear = costs::lin_capacity;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2.5;
};

/// stable_indexed_set.
template<typename T, typename Index>
struct container_costs<repr::StableIndexedSet<T, Index>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2.5;
};

/// concurrent_indexed_set.
template<typename T, typename Index>
struct container_costs<repr::ConcurrentIndexedSet<T, Index>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2.6;
};

}  // namespace urd

namespace urd
{

template<typename T>
struct container_synopsis<repr::IndexedSet<T, std::uint32_t>>
{
    static constexpr std::string_view id = "indexed_set";
    static constexpr std::string_view description = "indexed set (a vector in insertion order, plus a hash table of positions)";
    static constexpr std::string_view holds = "n elements in order + a table of positions";
};

template<typename T, typename Index>
struct container_synopsis<repr::StableIndexedSet<T, Index>>
{
    static constexpr std::string_view id = "stable_indexed_set";
    static constexpr std::string_view description = "stable indexed set (segments that never move, a hash table of positions; no lock)";
    static constexpr std::string_view holds = "segments that never move + a table of positions";
};

template<typename T, typename Index>
struct container_synopsis<repr::ConcurrentIndexedSet<T, Index>>
{
    static constexpr std::string_view id = "concurrent_indexed_set";
    static constexpr std::string_view description = "concurrent indexed set (segments that never move, a concurrent table of positions)";
    static constexpr std::string_view holds = "segments that never move + a table of positions; new elements under a lock";
};

}  // namespace urd
