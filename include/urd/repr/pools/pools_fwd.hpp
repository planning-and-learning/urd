#pragma once

// Pools of rows and of slots, declared with their defaults and what the registry reads of them: the row interner
// (rows kept in full, each once), the tree pools (rows as trees whose equal subtrees are stored once; of values, or
// of bits), and the slot vector (elements by handle in slots that never move). Defined in urd/repr/pools/pools.hpp,
// tree_vector.hpp, tree_bitset.hpp and slots.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
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

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// row interner (each row once, in full)
struct row_interner_decl
{
    static constexpr std::string_view id = "row_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/pools/pools.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, memory::retains, threading::none });
    static constexpr role_tags relies_on = relies(traits<role::row_element, trait::can_hash>());
    static constexpr std::array<tag, 0> gives_elements {};
    struct operation_costs
    {
        static constexpr cost insert_row = costs::lin_row_e;
        static constexpr cost find_row = costs::lin_row_e;
        static constexpr cost access = costs::one;
        static constexpr double words = 2.5;
    };
    static constexpr std::string_view description = "row interner (each row once, in full)";
    static constexpr std::string_view holds = "each distinct row once, in full";
};
template<typename Row>
struct declaration_of<repr::RowInterner<Row, false>>
{
    using declaration = row_interner_decl;
    static constexpr role_tags roles = element_roles<Row>();
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
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, sharing::structural_sharing, memory::retains, threads(ThreadSafe) });
    /// Of bits: rows that are dense sets of indices. Of values: sequence rows of hashable elements.
    static constexpr role_tags relies_on = Bits ? relies(tags<role::row>({ uniqueness::unique, keys::dense }), traits<role::row_element, trait::an_index>())
                                                : relies(tags<role::row>({ position::positional }), traits<role::row_element, trait::can_hash>());
    static constexpr std::array<tag, 0> gives_elements {};
    struct operation_costs
    {
        static constexpr cost insert_row = Bits ? costs::lin_row : costs::lin_row_e;
        static constexpr cost find_row = Bits ? costs::lin_row : costs::lin_row_e;
        static constexpr cost access = costs::lin_row;
        static constexpr double words = Bits ? 0.05 : 1;
    };
    static constexpr std::string_view description =
        Bits ? "tree bitset set (sets of indices as trees of blocks, equal subtrees once)" : "tree vector set (rows as trees of values, equal subtrees once)";
    static constexpr std::string_view holds =
        Bits ? "nodes shared across rows; all-zero subtrees take nothing" : "nodes shared across rows: a delta adds about k log r nodes";
};
template<typename Row, std::size_t Arity, bool ThreadSafe>
struct declaration_of<repr::TreeBitsetSet<Row, Arity, ThreadSafe>>
{
    using declaration = tree_pool_decl<true, ThreadSafe>;
    static constexpr role_tags roles = element_roles<Row>();
};
template<typename Row, std::size_t Arity, bool ThreadSafe>
struct declaration_of<repr::TreeVectorSet<Row, Arity, ThreadSafe>>
{
    using declaration = tree_pool_decl<false, ThreadSafe>;
    static constexpr role_tags roles = element_roles<Row>();
};

}  // namespace detail

namespace detail
{

/// row interner, thread-safe
struct thread_safe_row_interner_decl
{
    static constexpr std::string_view id = "thread_safe_row_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/pools/pools.hpp" };
    using kind = pool_kind;
    static constexpr property_set provides = property_set::of({ handling::handles, uniqueness::unique, memory::retains, threading::concurrent });
    static constexpr role_tags relies_on = relies(traits<role::row_element, trait::can_hash>());
    static constexpr std::array<tag, 0> gives_elements {};
    struct operation_costs
    {
        static constexpr cost insert_row = costs::lin_row_e;
        static constexpr cost find_row = costs::lin_row_e;
        static constexpr cost access = costs::one;
        static constexpr double words = 2.6;
    };
    static constexpr std::string_view description = "row interner, thread-safe";
    static constexpr std::string_view holds = "each distinct row once, in full; inserts under a lock";
};
template<typename Row>
struct declaration_of<repr::RowInterner<Row, true>>
{
    using declaration = thread_safe_row_interner_decl;
    static constexpr role_tags roles = element_roles<Row>();
};

}  // namespace detail

}  // namespace urd
