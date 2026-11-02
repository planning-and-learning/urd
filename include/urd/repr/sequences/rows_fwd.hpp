#pragma once

// Representations of nested Stores, whose elements (rows) are Stores themselves, declared with what the registry
// reads of them. Defined in urd/repr/sequences/jagged.hpp and urd/repr/sequences/row_pool.hpp.

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <type_traits>

namespace urd::repr
{

/// Rows stored flattened, as CSR: offsets into one array of the elements. Frozen rows are read-only slices; rows of a
/// fixed shape change in place, and with TrackRows reset touches only the rows handed out.
template<typename Inner, bool TrackRows = false>
class Jagged;

/// Rows kept for reuse: removing a row clears it instead of destroying it.
template<typename Inner>
class RowPool;

}  // namespace urd::repr

namespace urd::trait
{

// What rows T (Stores held as elements, or registered containers of elements) allow: the conditions of the row
// containers declared below, each its words and what it means (holds).

struct rows_flatten
{
    static constexpr std::string_view name = "rows_flatten";
    static constexpr std::string_view description = "rows frozen, holding elements or rows kept in order: stored flattened";
    /// Frozen rows, whose elements are no rows or which keep their rows where they are put (a sequence of rows).
    template<typename T>
    static consteval bool holds()
    {
        using row = detail::held_as<T>;
        if constexpr (!row::row)
            return false;
        else if constexpr (!detail::held_as<typename row::elements>::row)
            return row::properties.has(change::frozen);
        else
            return row::properties.has(change::frozen) && detail::appends<typename row::kind>;
    }
};
struct rows_in_place
{
    static constexpr std::string_view name = "rows_in_place";
    static constexpr std::string_view description = "rows changed in place: sequences, dense sets' bits, or fixed-shape rows of such rows";
    /// Rows not frozen that change in place by their kind, or of a fixed shape over such rows, at any depth.
    template<typename T>
    static consteval bool holds()
    {
        using row = detail::held_as<T>;
        if constexpr (!row::row)
            return false;
        else if constexpr (!detail::held_as<typename row::elements>::row)
            return !row::properties.has(change::frozen) && row::kind::changes_in_place(row::properties);
        else
            return !row::properties.has(change::frozen) && row::properties.has(shape::fixed_shape) && holds<typename row::elements>();
    }
};
struct rows_reused
{
    static constexpr std::string_view name = "rows_reused";
    static constexpr std::string_view description = "rows that may shrink, so emptied ones can be kept for reuse";
    template<typename T>
    static consteval bool holds()
    {
        using row = detail::held_as<T>;
        return row::row && !row::properties.has(change::monotone) && !row::properties.has(emptiness::nonempty) && !row::properties.has(shape::fixed_shape);
    }
};

}  // namespace urd::trait

template<>
struct urd::traits_of_role<urd::role::rows>
{
    using type = detail::type_list<trait::rows_flatten, trait::rows_in_place, trait::rows_reused>;
};

namespace urd
{

namespace detail
{

struct jagged_decl
{
    static constexpr std::string_view id = "jagged";
    static constexpr auto headers = header_list<1> { "urd/repr/sequences/jagged.hpp" };
    using kind = sequence_kind;
    static constexpr property_set provides = property_set::of({ position::positional, memory::retains, clearing::proportional_clear });
    static constexpr property_set relies_on = relies(traits<role::rows, trait::rows_flatten>());
    /// Rows move with the data array and keep its memory; laid out contiguously where they hold plain elements.
    static constexpr std::array<tag, 3> gives_elements { layout::none, referencing::none, memory::retains };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::lin_row>,
                                       cost_entry<operation::erase, costs::lin_n>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>,
                                       cost_entry<operation::access, costs::one>>;
    static constexpr double words = 1.2;
    static constexpr std::string_view description = "jagged array (CSR: offsets + one data array)";
    static constexpr std::string_view holds = "one offsets array per level";
};

struct fixed_jagged_decl
{
    static constexpr std::string_view id = "fixed_jagged";
    static constexpr auto headers = header_list<1> { "urd/repr/sequences/jagged.hpp" };
    using kind = sequence_kind;
    static constexpr property_set provides = property_set::of({ position::positional, shape::fixed_shape, memory::retains, clearing::none });
    static constexpr property_set relies_on = relies(stated({ shape::fixed_shape }), traits<role::rows, trait::rows_in_place>());
    /// Rows move with the data array and keep its memory; laid out contiguously where they are sequences of elements.
    static constexpr std::array<tag, 3> gives_elements { layout::none, referencing::none, memory::retains };
    using operation_costs = cost_table<cost_entry<operation::iterate, costs::lin_n>, cost_entry<operation::access, costs::one>>;
    static constexpr double words = 1.2;
    static constexpr std::string_view description = "jagged array of rows changed in place (fixed shape: offsets + one data array)";
    static constexpr std::string_view holds = "one offsets array per level";
};
template<typename Inner>
struct declaration_of<repr::Jagged<Inner>>
{
    using declaration = std::conditional_t<trait::rows_flatten::holds<Inner>(), jagged_decl, fixed_jagged_decl>;
    static constexpr property_set roles = roles_of<urd::sequence<Inner>>();
};

}  // namespace detail

namespace detail
{

struct row_pool_decl
{
    static constexpr std::string_view id = "row_pool";
    static constexpr auto headers = header_list<1> { "urd/repr/sequences/row_pool.hpp" };
    using kind = sequence_kind;
    static constexpr property_set provides = property_set::of({ position::positional, layout::contiguous, memory::retains, clearing::proportional_clear });
    static constexpr property_set relies_on = relies(traits<role::rows, trait::rows_reused>());
    static constexpr std::array<tag, 0> gives_elements {};
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_a>,
                                       cost_entry<operation::erase, costs::one>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>,
                                       cost_entry<operation::access, costs::one>>;
    static constexpr double words = 4;
    static constexpr std::string_view description = "row pool (cleared rows are kept for reuse)";
    static constexpr std::string_view holds = "rows kept with their storage";
};
template<typename Inner>
struct declaration_of<repr::RowPool<Inner>>
{
    using declaration = row_pool_decl;
    static constexpr property_set roles = roles_of<urd::sequence<Inner>>();
};

}  // namespace detail

namespace detail
{

struct tracked_jagged_decl
{
    static constexpr std::string_view id = "tracked_jagged";
    static constexpr auto headers = header_list<1> { "urd/repr/sequences/jagged.hpp" };
    using kind = sequence_kind;
    static constexpr property_set provides = property_set::of({ position::positional, shape::fixed_shape, memory::retains, clearing::proportional_clear });
    static constexpr property_set relies_on = relies(stated({ shape::fixed_shape }), traits<role::rows, trait::rows_in_place>());
    /// Rows move with the data array and keep its memory; laid out contiguously where they are sequences of elements.
    static constexpr std::array<tag, 3> gives_elements { layout::none, referencing::none, memory::retains };
    using operation_costs = cost_table<cost_entry<operation::iterate, costs::lin_n>, cost_entry<operation::access, costs::one>>;
    static constexpr double words = 1.3;
    static constexpr std::string_view description = "jagged rows that track the rows handed out to change";
    static constexpr std::string_view holds = "one array, + the rows handed out to change";
};
template<typename Inner>
struct declaration_of<repr::Jagged<Inner, true>>
{
    using declaration = tracked_jagged_decl;
    static constexpr property_set roles = roles_of<urd::sequence<Inner>>();
};

}  // namespace detail

}  // namespace urd
