#pragma once

// A set kept in insertion order whose positions are its elements' indices (an interner), declared with its defaults
// and what the registry reads of it. Defined in urd/repr/indexed.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

#include <cstdint>

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P indexed_set(const T& t)
{
    if (!t.hashable)
        return {};
    return props({ tag::insertion_ordered, tag::unique, tag::positional, tag::contiguous, tag::shrinkable, tag::retains });
}

constexpr P stable_indexed_set(const T& t, bool concurrent_)
{
    if (!t.hashable)
        return {};
    return with(props({ tag::insertion_ordered, tag::unique, tag::positional, tag::stable_refs, tag::growable, tag::retains }), { threads(concurrent_) });
}

}  // namespace urd::detail::guarantees


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

namespace detail
{

/// indexed set (a vector in insertion order, plus a hash table of positions)
struct indexed_set_decl
{
    static constexpr std::string_view id = "indexed_set";
    static constexpr auto headers = header_list<1> { "urd/repr/indexed.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::indexed_set(f.element);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_ae;
        static constexpr cost lookup = costs::one_e;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_capacity;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 2.5;
    };
    static constexpr std::string_view description = "indexed set (a vector in insertion order, plus a hash table of positions)";
    static constexpr std::string_view holds = "n elements in order + a table of positions";
};
template<typename T>
struct declaration_of<repr::IndexedSet<T, std::uint32_t>>
{
    using declaration = indexed_set_decl;
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

namespace detail
{

/// stable indexed set (segments that never move, a hash table of positions; no lock)
struct stable_indexed_set_decl
{
    static constexpr std::string_view id = "stable_indexed_set";
    static constexpr auto headers = header_list<1> { "urd/repr/indexed.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::stable_indexed_set(f.element, false);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_ae;
        static constexpr cost lookup = costs::one_e;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 2.5;
    };
    static constexpr std::string_view description = "stable indexed set (segments that never move, a hash table of positions; no lock)";
    static constexpr std::string_view holds = "segments that never move + a table of positions";
};
template<typename T, typename Index>
struct declaration_of<repr::StableIndexedSet<T, Index>>
{
    using declaration = stable_indexed_set_decl;
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

namespace detail
{

/// concurrent indexed set (segments that never move, a concurrent table of positions)
struct concurrent_indexed_set_decl
{
    static constexpr std::string_view id = "concurrent_indexed_set";
    static constexpr auto headers = header_list<1> { "urd/repr/indexed.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::stable_indexed_set(f.element, true);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_ae;
        static constexpr cost lookup = costs::one_e;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 2.6;
    };
    static constexpr std::string_view description = "concurrent indexed set (segments that never move, a concurrent table of positions)";
    static constexpr std::string_view holds = "segments that never move + a table of positions; new elements under a lock";
};
template<typename T, typename Index>
struct declaration_of<repr::ConcurrentIndexedSet<T, Index>>
{
    using declaration = concurrent_indexed_set_decl;
    static constexpr request_features features = element_facts<T>();
};

}  // namespace detail

}  // namespace urd
