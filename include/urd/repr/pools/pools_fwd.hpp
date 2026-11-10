#pragma once

// Pools of rows and of slots, declared with their defaults and what the registry reads of them: the row interner
// (rows kept in full, each once), the tree pools (rows as trees whose equal subtrees are stored once; of values, or
// of bits), and the slot vector (elements by handle in slots that never move). Defined in urd/repr/pools/pools.hpp,
// tree_vector.hpp, tree_bitset.hpp and slots.hpp.

#include "urd/cost.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <bit>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace urd::repr
{

/// A pool of rows (Row: a Store, or any range constructible from its elements), each stored once and in full; with
/// Width bit, rows of indices, each element in a bit width given at construction (urd::bit); with Width by_column_widths, rows
/// of unsigned columns stored by column widths given at construction (urd::by_column_widths). H is the index it hands out
/// (urd::pool<V, H>).
template<typename Row, bool ThreadSafe = false, urd::width Width = urd::width::word, typename H = urd::index<Row>>
class RowInterner;

/// A pool of sequence rows, each a balanced tree with Arity children per node, equal subtrees stored once.
template<typename Row, std::size_t Arity = 4, bool ThreadSafe = false>
    requires(std::has_single_bit(Arity) && Arity >= 2)
class TreeVectorSet;

/// A pool of rows that are sets of indices, each a tree of 32-bit blocks, equal subtrees stored once.
template<typename Row, std::size_t Arity = 4, bool ThreadSafe = false>
    requires(std::has_single_bit(Arity) && Arity >= 2)
class TreeBitsetSet;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

struct row_interner_decl
{
    static constexpr std::string_view id = "row_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/pools/pools.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, memory::retains, threading::none, room::reservable });
    static constexpr property_set relies_on = relies(traits<role::row_element, trait::can_hash>());
    static constexpr std::array<tag, 0> gives_elements {};
    using operation_costs = cost_table<cost_entry<operation::insert_row, costs::lin_row_e>,
                                       cost_entry<operation::find_row, costs::lin_row_e>,
                                       cost_entry<operation::access, costs::one>>;
    static constexpr double words = 4.5;
    static constexpr std::string_view description = "row interner (each row once, in full)";
    static constexpr std::string_view holds = "each distinct row once, in full";
};
template<typename Row, typename H>
struct declaration_of<repr::RowInterner<Row, false, urd::width::word, H>>
{
    using declaration = row_interner_decl;
    static constexpr property_set roles = roles_of<urd::pool<Row, H>>();
};

/// A row interner of rows of indices whose request promises urd::bit: each element in the bit width given at
/// construction, rows of one length packed back to back; thread-safe or not.
template<bool ThreadSafe>
struct bit_row_interner_decl : row_interner_decl
{
    static constexpr std::string_view id = ThreadSafe ? "thread_safe_bit_row_interner" : "bit_row_interner";
    static constexpr property_set provides =
        property_set::of({ handling::handles, uniqueness::unique, memory::retains, ThreadSafe ? tag(threading::concurrent) : tag(threading::none), room::reservable });
    static constexpr property_set relies_on = relies(stated({ width::bit }),
                                                     tags<role::row>({ position::positional }),
                                                     traits<role::row_element, trait::an_index>(),
                                                     traits<role::row_element, trait::can_hash>());
    static constexpr double words = ThreadSafe ? 1.1 : 0.7;
    static constexpr std::string_view description = "bit row interner (rows of indices, each element in a bit width given at construction)";
    static constexpr std::string_view holds = "each distinct row once, its elements in the given bits, rows of one length back to back";
};
template<typename Row, bool ThreadSafe, typename H>
struct declaration_of<repr::RowInterner<Row, ThreadSafe, urd::width::bit, H>>
{
    using declaration = bit_row_interner_decl<ThreadSafe>;
    static constexpr property_set roles = roles_of<urd::pool<Row, H>>();
};

/// A row interner of rows whose request promises urd::by_column_widths: unsigned columns, each in the bits the column
/// widths given at construction give it, rows back to back; thread-safe or not.
template<bool ThreadSafe>
struct schema_row_interner_decl : bit_row_interner_decl<ThreadSafe>
{
    static constexpr std::string_view id = ThreadSafe ? "thread_safe_schema_row_interner" : "schema_row_interner";
    static constexpr property_set relies_on = relies(stated({ width::by_column_widths }), tags<role::row>({ position::positional }));
    static constexpr std::string_view description = "schema row interner (rows of unsigned columns, each column in the bits the widths given at construction give it)";
    static constexpr std::string_view holds = "each distinct row once, its columns in their widths' bits, rows back to back";
};
template<typename Row, bool ThreadSafe, typename H>
struct declaration_of<repr::RowInterner<Row, ThreadSafe, urd::width::by_column_widths, H>>
{
    using declaration = schema_row_interner_decl<ThreadSafe>;
    static constexpr property_set roles = roles_of<urd::pool<Row, H>>();
};

}  // namespace detail

namespace detail
{

/// A pool of rows as trees, equal subtrees stored once: of values (`Bits` false) or of index sets' bits (true),
/// thread-safe or not.
template<bool Bits, bool ThreadSafe>
struct tree_pool_decl
{
    static constexpr std::string_view id =
        Bits ? (ThreadSafe ? "thread_safe_tree_bitset_set" : "tree_bitset_set") : (ThreadSafe ? "thread_safe_tree_vector_set" : "tree_vector_set");
    static constexpr auto headers = header_list<1> { Bits ? "urd/repr/pools/tree_bitset.hpp" : "urd/repr/pools/tree_vector.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides = property_set::of({ handling::handles,
                                                                uniqueness::unique,
                                                                sharing::structural_sharing,
                                                                memory::retains,
                                                                ThreadSafe ? tag(threading::concurrent) : tag(threading::none) });
    /// Of bits: rows that are dense sets of indices. Of values: sequence rows of hashable elements.
    static constexpr property_set relies_on = Bits ?
                                                  relies(tags<role::row>({ uniqueness::unique, keys::dense }), traits<role::row_element, trait::an_index>()) :
                                                  relies(tags<role::row>({ position::positional }), traits<role::row_element, trait::can_hash>());
    static constexpr std::array<tag, 0> gives_elements {};
    using operation_costs = cost_table<cost_entry<operation::insert_row, Bits ? costs::lin_row : costs::lin_row_e>,
                                       cost_entry<operation::find_row, Bits ? costs::lin_row : costs::lin_row_e>,
                                       cost_entry<operation::access, costs::lin_row>>;
    static constexpr double words = Bits ? (ThreadSafe ? 0.8 : 0.2) : (ThreadSafe ? 1.8 : 1);
    static constexpr std::string_view description =
        Bits ? "tree bitset set (sets of indices as trees of blocks, equal subtrees once)" : "tree vector set (rows as trees of values, equal subtrees once)";
    static constexpr std::string_view holds =
        Bits ? "nodes shared across rows; all-zero subtrees take nothing" : "nodes shared across rows: a delta adds about k log r nodes";
};
template<typename Row, std::size_t Arity, bool ThreadSafe>
struct declaration_of<repr::TreeBitsetSet<Row, Arity, ThreadSafe>>
{
    using declaration = tree_pool_decl<true, ThreadSafe>;
    static constexpr property_set roles = roles_of<urd::pool<Row>>();
};
template<typename Row, std::size_t Arity, bool ThreadSafe>
struct declaration_of<repr::TreeVectorSet<Row, Arity, ThreadSafe>>
{
    using declaration = tree_pool_decl<false, ThreadSafe>;
    static constexpr property_set roles = roles_of<urd::pool<Row>>();
};

}  // namespace detail

namespace detail
{

struct thread_safe_row_interner_decl : row_interner_decl
{
    static constexpr std::string_view id = "thread_safe_row_interner";
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, memory::retains, threading::concurrent, room::reservable });
    static constexpr double words = 4.1;
    static constexpr std::string_view description = "row interner, thread-safe";
    static constexpr std::string_view holds = "each distinct row once, in full; inserts under a lock";
};
template<typename Row, typename H>
struct declaration_of<repr::RowInterner<Row, true, urd::width::word, H>>
{
    using declaration = thread_safe_row_interner_decl;
    static constexpr property_set roles = roles_of<urd::pool<Row, H>>();
};

}  // namespace detail

}  // namespace urd
