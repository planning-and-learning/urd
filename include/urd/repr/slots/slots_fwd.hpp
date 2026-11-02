#pragma once

// Pools of rows and of slots, declared with their defaults and what the registry reads of them: the row interner
// (rows kept in full, each once), the tree pools (rows as trees whose equal subtrees are stored once; of values, or
// of bits), and the slot vector (elements by handle in slots that never move). Defined in urd/repr/pools/pools.hpp,
// tree_vector.hpp, tree_bitset.hpp and slots.hpp.

#include "urd/cost.hpp"
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

/// Elements in slots that never move, the slots of erased elements reused.
template<typename T, bool ThreadSafe = false>
class SlotVector;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

struct slot_vector_decl
{
    static constexpr std::string_view id = "slot_vector";
    static constexpr auto headers = header_list<1> { "urd/repr/slots/slots.hpp" };
    using kind = slots_kind;
    static constexpr property_set provides = property_set::of({ handling::handles, referencing::stable_refs, memory::retains, threading::none });
    static constexpr property_set relies_on {};
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert_row, costs::one_a>, cost_entry<operation::access, costs::one>>;
    static constexpr double words = 1.5;
    static constexpr std::string_view description = "slot vector (slots that never move, freed ones reused)";
    static constexpr std::string_view holds = "stable slots; freed slots reused first";
};
template<typename T>
struct declaration_of<repr::SlotVector<T, false>>
{
    using declaration = slot_vector_decl;
    static constexpr property_set roles = roles_of<urd::slots<T>>();
};

}  // namespace detail

namespace detail
{

struct thread_safe_slot_vector_decl
{
    static constexpr std::string_view id = "thread_safe_slot_vector";
    static constexpr auto headers = header_list<1> { "urd/repr/slots/slots.hpp" };
    using kind = slots_kind;
    static constexpr property_set provides = property_set::of({ handling::handles, referencing::stable_refs, memory::retains, threading::concurrent });
    static constexpr property_set relies_on {};
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert_row, costs::one_a>, cost_entry<operation::access, costs::one>>;
    static constexpr double words = 1.6;
    static constexpr std::string_view description = "slot vector (slots that never move, freed ones reused)";
    static constexpr std::string_view holds = "stable slots; freed slots reused first, under a lock";
};
template<typename T>
struct declaration_of<repr::SlotVector<T, true>>
{
    using declaration = thread_safe_slot_vector_decl;
    static constexpr property_set roles = roles_of<urd::slots<T>>();
};

}  // namespace detail

}  // namespace urd
