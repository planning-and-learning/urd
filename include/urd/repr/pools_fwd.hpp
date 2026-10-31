#pragma once

// Pools of rows and of slots, declared with their defaults and what the registry reads of them: the row interner
// (rows kept in full, each once), the tree pools (rows as trees whose equal subtrees are stored once; of values, or
// of bits), and the slot vector (elements by handle in slots that never move). Defined in urd/repr/pools.hpp,
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

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P row_interner(const T& row, bool concurrent_)
{
    if (row.element == nullptr || !row.element->hashable)
        return {};
    return property_set::of({ tag::handles, tag::unique, tag::shrinkable, tag::retains, threads(concurrent_) });
}

constexpr P tree_vector_set(const T& row, bool concurrent_)
{
    if (row.element == nullptr || !row.element->hashable || (row.store != nullptr && keeps_by_value(row.store->kind)))
        return {};
    return property_set::of({ tag::handles, tag::unique, tag::structural_sharing, tag::shrinkable, tag::retains, threads(concurrent_) });
}

constexpr P tree_bitset_set(const T& row, bool concurrent_)
{
    if (row.store == nullptr || row.element == nullptr || !row.element->indexable || !row.store->properties.has(tag::unique) || !row.store->properties.has(tag::dense))
        return {};
    return property_set::of({ tag::handles, tag::unique, tag::structural_sharing, tag::shrinkable, tag::retains, threads(concurrent_) });
}

constexpr P slot_vector(bool concurrent_) { return property_set::of({ tag::handles, tag::stable_refs, tag::shrinkable, tag::retains, threads(concurrent_) }); }

}  // namespace urd::detail::guarantees


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

namespace detail
{

/// row interner (each row once, in full)
struct row_interner_decl
{
    static constexpr std::string_view id = "row_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/pools.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::row_interner(f.element, false);
        if (!held)
            return {};
        auto result = offer_of(urd::kind::pool, *held, f);
        if (const auto nested = guarantees::own_rows(f.element))
            result = result.giving_nested(*nested);
        return result;
    }
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
    static constexpr request_features features = element_facts<Row>();
};

}  // namespace detail

namespace detail
{

/// slot vector (slots that never move, freed ones reused)
struct slot_vector_decl
{
    static constexpr std::string_view id = "slot_vector";
    static constexpr auto headers = header_list<1> { "urd/repr/slots.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::slot_vector(false);
        if (!held)
            return {};
        return offer_of(urd::kind::pool, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert_row = costs::one_a;
        static constexpr cost access = costs::one;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "slot vector (slots that never move, freed ones reused)";
    static constexpr std::string_view holds = "stable slots; freed slots reused first";
};
template<typename T>
struct declaration_of<repr::SlotVector<T, false>>
{
    using declaration = slot_vector_decl;
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

namespace detail
{

/// slot vector (slots that never move, freed ones reused)
struct thread_safe_slot_vector_decl
{
    static constexpr std::string_view id = "thread_safe_slot_vector";
    static constexpr auto headers = header_list<1> { "urd/repr/slots.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::slot_vector(true);
        if (!held)
            return {};
        return offer_of(urd::kind::pool, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert_row = costs::one_a;
        static constexpr cost access = costs::one;
        static constexpr double words = 1.6;
    };
    static constexpr std::string_view description = "slot vector (slots that never move, freed ones reused)";
    static constexpr std::string_view holds = "stable slots; freed slots reused first, under a lock";
};
template<typename T>
struct declaration_of<repr::SlotVector<T, true>>
{
    using declaration = thread_safe_slot_vector_decl;
    static constexpr request_features features = element_facts<T>();
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
    static constexpr auto headers = header_list<1> { Bits ? "urd/repr/tree_bitset.hpp" : "urd/repr/tree_vector.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = Bits ? guarantees::tree_bitset_set(f.element, ThreadSafe) : guarantees::tree_vector_set(f.element, ThreadSafe);
        if (!held)
            return {};
        auto result = offer_of(urd::kind::pool, *held, f);
        if (const auto nested = guarantees::own_rows(f.element))
            result = result.giving_nested(*nested);
        return result;
    }
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
    static constexpr request_features features = element_facts<Row>();
};
template<typename Row, std::size_t Arity, bool ThreadSafe>
struct declaration_of<repr::TreeVectorSet<Row, Arity, ThreadSafe>>
{
    using declaration = tree_pool_decl<false, ThreadSafe>;
    static constexpr request_features features = element_facts<Row>();
};

}  // namespace detail

namespace detail
{

/// row interner, thread-safe
struct thread_safe_row_interner_decl
{
    static constexpr std::string_view id = "thread_safe_row_interner";
    static constexpr auto headers = header_list<1> { "urd/repr/pools.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::row_interner(f.element, true);
        if (!held)
            return {};
        auto result = offer_of(urd::kind::pool, *held, f);
        if (const auto nested = guarantees::own_rows(f.element))
            result = result.giving_nested(*nested);
        return result;
    }
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
    static constexpr request_features features = element_facts<Row>();
};

}  // namespace detail

}  // namespace urd
