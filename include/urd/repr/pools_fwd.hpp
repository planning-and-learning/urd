#pragma once

// Pools of rows and of slots, declared with their defaults and what the registry reads of them: the row interner
// (rows kept in full, each once), the tree pools (rows as trees whose equal subtrees are stored once; of values, or
// of bits), and the slot vector (elements by handle in slots that never move). Defined in urd/repr/pools.hpp,
// tree_vector.hpp, tree_bitset.hpp and slots.hpp, which urd/use/pools.hpp includes.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
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

namespace urd
{

/// row_interner.
template<typename Row>
struct container_costs<repr::RowInterner<Row, false>>
{
    static constexpr cost insert_row = costs::lin_row_e;
    static constexpr cost find_row = costs::lin_row_e;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2.5;
};

/// slot_vector.
template<typename T>
struct container_costs<repr::SlotVector<T, false>>
{
    static constexpr cost insert_row = costs::one_at;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 1.5;
};

/// thread_safe_slot_vector.
template<typename T>
struct container_costs<repr::SlotVector<T, true>>
{
    static constexpr cost insert_row = costs::one_at;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 1.6;
};

/// tree_bitset_set.
template<typename Row, std::size_t Arity, bool ThreadSafe>
struct container_costs<repr::TreeBitsetSet<Row, Arity, ThreadSafe>>
{
    static constexpr cost insert_row = costs::lin_row_t;
    static constexpr cost find_row = costs::lin_row_t;
    static constexpr cost access = costs::lin_row_t;
    static constexpr double words = 0.05;
};

/// tree_vector_set.
template<typename Row, std::size_t Arity, bool ThreadSafe>
struct container_costs<repr::TreeVectorSet<Row, Arity, ThreadSafe>>
{
    static constexpr cost insert_row = costs::lin_row_e;
    static constexpr cost find_row = costs::lin_row_e;
    static constexpr cost access = costs::lin_row_t;
    static constexpr double words = 1;
};

/// thread_safe_row_interner.
template<typename Row>
struct container_costs<repr::RowInterner<Row, true>>
{
    static constexpr cost insert_row = costs::lin_row_e;
    static constexpr cost find_row = costs::lin_row_e;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2.6;
};

}  // namespace urd

namespace urd
{

template<typename Row>
struct container_synopsis<repr::RowInterner<Row, false>>
{
    static constexpr std::string_view id = "row_interner";
    static constexpr std::string_view description = "row interner (each row once, in full)";
    static constexpr std::string_view holds = "each distinct row once, in full";
};

template<typename T>
struct container_synopsis<repr::SlotVector<T, false>>
{
    static constexpr std::string_view id = "slot_vector";
    static constexpr std::string_view description = "slot vector (slots that never move, freed ones reused)";
    static constexpr std::string_view holds = "stable slots; freed slots reused first";
};

template<typename T>
struct container_synopsis<repr::SlotVector<T, true>>
{
    static constexpr std::string_view id = "thread_safe_slot_vector";
    static constexpr std::string_view description = "slot vector (slots that never move, freed ones reused)";
    static constexpr std::string_view holds = "stable slots; freed slots reused first, under a lock";
};

template<typename Row, std::size_t Arity, bool ThreadSafe>
struct container_synopsis<repr::TreeBitsetSet<Row, Arity, ThreadSafe>>
{
    static constexpr std::string_view id = "tree_bitset_set";
    static constexpr std::string_view description = "tree bitset set (sets of indices as trees of blocks, equal subtrees once)";
    static constexpr std::string_view holds = "nodes shared across rows; all-zero subtrees take nothing";
};

template<typename Row, std::size_t Arity, bool ThreadSafe>
struct container_synopsis<repr::TreeVectorSet<Row, Arity, ThreadSafe>>
{
    static constexpr std::string_view id = "tree_vector_set";
    static constexpr std::string_view description = "tree vector set (rows as trees of values, equal subtrees once)";
    static constexpr std::string_view holds = "nodes shared across rows: a delta adds about k log r nodes";
};

template<typename Row>
struct container_synopsis<repr::RowInterner<Row, true>>
{
    static constexpr std::string_view id = "thread_safe_row_interner";
    static constexpr std::string_view description = "row interner, thread-safe";
    static constexpr std::string_view holds = "each distinct row once, in full; inserts under a lock";
};

}  // namespace urd
