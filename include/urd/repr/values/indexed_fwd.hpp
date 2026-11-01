#pragma once

// A set kept in insertion order whose positions are its elements' indices (an interner), declared with its defaults
// and what the registry reads of it. Defined in urd/repr/values/indexed.hpp.

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

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

namespace detail
{

/// indexed set (a vector in insertion order, plus a hash table of positions)
struct indexed_set_decl
{
    static constexpr std::string_view id = "indexed_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/indexed.hpp" };
    using kind = set_kind;
    static constexpr property_set provides =
        property_set::of({ order::insertion_ordered, uniqueness::unique, position::positional, layout::contiguous, memory::retains });
    static constexpr property_set relies_on = relies(traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_ae>,
                                       cost_entry<operation::lookup, costs::one_e>,
                                       cost_entry<operation::erase, costs::lin_n>,
                                       cost_entry<operation::clear, costs::lin_capacity>,
                                       cost_entry<operation::iterate, costs::lin_n>,
                                       cost_entry<operation::access, costs::one>>;
    static constexpr double words = 2.5;
    static constexpr std::string_view description = "indexed set (a vector in insertion order, plus a hash table of positions)";
    static constexpr std::string_view holds = "n elements in order + a table of positions";
};
template<typename T>
struct declaration_of<repr::IndexedSet<T, std::uint32_t>>
{
    using declaration = indexed_set_decl;
    static constexpr property_set roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// stable indexed set (segments that never move, a hash table of positions; no lock)
struct stable_indexed_set_decl
{
    static constexpr std::string_view id = "stable_indexed_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/indexed.hpp" };
    using kind = set_kind;
    static constexpr property_set provides =
        property_set::of({ order::insertion_ordered, uniqueness::unique, position::positional, referencing::stable_refs, memory::retains, threading::none });
    static constexpr property_set relies_on = relies(stated({ change::monotone }), traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_ae>,
                                       cost_entry<operation::lookup, costs::one_e>,
                                       cost_entry<operation::iterate, costs::lin_n>,
                                       cost_entry<operation::access, costs::one>>;
    static constexpr double words = 2.5;
    static constexpr std::string_view description = "stable indexed set (segments that never move, a hash table of positions; no lock)";
    static constexpr std::string_view holds = "segments that never move + a table of positions";
};
template<typename T, typename Index>
struct declaration_of<repr::StableIndexedSet<T, Index>>
{
    using declaration = stable_indexed_set_decl;
    static constexpr property_set roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// concurrent indexed set (segments that never move, a concurrent table of positions)
struct concurrent_indexed_set_decl
{
    static constexpr std::string_view id = "concurrent_indexed_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/indexed.hpp" };
    using kind = set_kind;
    static constexpr property_set provides = property_set::of(
        { order::insertion_ordered, uniqueness::unique, position::positional, referencing::stable_refs, memory::retains, threading::concurrent });
    static constexpr property_set relies_on = relies(stated({ change::monotone }), traits<role::element, trait::can_hash>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_ae>,
                                       cost_entry<operation::lookup, costs::one_e>,
                                       cost_entry<operation::iterate, costs::lin_n>,
                                       cost_entry<operation::access, costs::one>>;
    static constexpr double words = 2.6;
    static constexpr std::string_view description = "concurrent indexed set (segments that never move, a concurrent table of positions)";
    static constexpr std::string_view holds = "segments that never move + a table of positions; new elements under a lock";
};
template<typename T, typename Index>
struct declaration_of<repr::ConcurrentIndexedSet<T, Index>>
{
    using declaration = concurrent_indexed_set_decl;
    static constexpr property_set roles = element_roles<T>();
};

}  // namespace detail

}  // namespace urd
