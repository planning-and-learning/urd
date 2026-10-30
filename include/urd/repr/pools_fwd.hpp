#pragma once

// Pools of rows and of slots, declared with their defaults and what the registry reads of them: the row interner
// (rows kept in full, each once), the tree pools (rows as trees whose equal subtrees are stored once; of values, or
// of bits), and the slot vector (elements by handle in slots that never move). Defined in urd/repr/pools.hpp,
// tree_vector.hpp, tree_bitset.hpp and slots.hpp, which urd/use/pools.hpp includes.

#include "urd/elements.hpp"
#include "urd/interface.hpp"
#include "urd/traits.hpp"

#include <bit>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace urd::repr
{

/// A pool of rows (Row: a Store, or any range constructible from its elements), each stored once and in full.
template<typename Row, bool ThreadSafe = false>
class RowInterner;
/// A pool of sequence rows, each a balanced tree with Arity children per node, equal subtrees stored once.
template<typename Row, std::size_t Arity = 4, bool ThreadSafe = false>
    requires(std::has_single_bit(Arity) && Arity >= 2)
class TreeVectorSet;
/// A pool of rows that are sets of indices, each a tree of 32-bit blocks, equal subtrees stored once.
template<typename Row, std::size_t Arity = 4, bool ThreadSafe = false>
    requires(std::has_single_bit(Arity) && Arity >= 2)
class TreeBitsetSet;
/// Elements in slots that never move, the slots of erased elements reused.
template<typename T, bool ThreadSafe = false>
class SlotVector;

}  // namespace urd::repr

namespace urd
{

/// Rows kept in full, each once; clear keeps the storage.
template<typename Row, bool ThreadSafe>
    requires hashable<std::vector<detail::row_element_t<Row>>>
struct container_traits<repr::RowInterner<Row, ThreadSafe>> :
    properties_of<handles, unique, shrinkable, retains, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

/// Its rows have the properties Row has: they come back as Rows.
template<typename Row, bool ThreadSafe>
struct element_traits<repr::RowInterner<Row, ThreadSafe>> : container_traits<Row>
{
};

/// Rows of hashable values, sharing equal subtrees; clear keeps the tables' storage.
template<typename Row, std::size_t Arity, bool ThreadSafe>
    requires hashable<detail::row_element_t<Row>> && (!by_value<traits_t<Row>>)
struct container_traits<repr::TreeVectorSet<Row, Arity, ThreadSafe>> :
    properties_of<handles, unique, structural_sharing, shrinkable, retains, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

template<typename Row, std::size_t Arity, bool ThreadSafe>
struct element_traits<repr::TreeVectorSet<Row, Arity, ThreadSafe>> : container_traits<Row>
{
};

/// Rows that are sets of indices, sharing equal subtrees; clear keeps the table's storage.
template<typename Row, std::size_t Arity, bool ThreadSafe>
    requires indexable<detail::row_element_t<Row>> && has<traits_t<Row>, unique, dense>
struct container_traits<repr::TreeBitsetSet<Row, Arity, ThreadSafe>> :
    properties_of<handles, unique, structural_sharing, shrinkable, retains, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

template<typename Row, std::size_t Arity, bool ThreadSafe>
struct element_traits<repr::TreeBitsetSet<Row, Arity, ThreadSafe>> : container_traits<Row>
{
};

/// Slots never move and are reused, not freed; with ThreadSafe, the operations above run concurrently.
template<typename T, bool ThreadSafe>
struct container_traits<repr::SlotVector<T, ThreadSafe>> :
    properties_of<handles, stable_refs, shrinkable, retains, std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

template<typename Row, bool ThreadSafe>
inline constexpr auto defined_in<repr::RowInterner<Row, ThreadSafe>> = detail::fixed_string("urd/use/pools.hpp");
template<typename Row, std::size_t Arity, bool ThreadSafe>
inline constexpr auto defined_in<repr::TreeVectorSet<Row, Arity, ThreadSafe>> = detail::fixed_string("urd/use/pools.hpp");
template<typename Row, std::size_t Arity, bool ThreadSafe>
inline constexpr auto defined_in<repr::TreeBitsetSet<Row, Arity, ThreadSafe>> = detail::fixed_string("urd/use/pools.hpp");
template<typename T, bool ThreadSafe>
inline constexpr auto defined_in<repr::SlotVector<T, ThreadSafe>> = detail::fixed_string("urd/use/pools.hpp");

}  // namespace urd
