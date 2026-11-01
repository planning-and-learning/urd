#pragma once

// Representations of nested Stores, whose elements (rows) are Stores themselves, declared with what the registry
// reads of them. Defined in urd/repr/sequences/jagged.hpp and urd/repr/sequences/row_pool.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

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

namespace urd
{

namespace detail
{

/// jagged array (CSR: offsets + one data array)
struct jagged_decl
{
    static constexpr std::string_view id = "jagged";
    static constexpr auto headers = header_list<1> { "urd/repr/sequences/jagged.hpp" };
    using kind = sequence_kind;
    static constexpr property_set provides = property_set::of({ position::positional, memory::retains, clearing::proportional_clear });
    static constexpr role_tags relies_on = relies(traits<role::rows, trait::rows_flatten>());
    /// Rows move with the data array and keep its memory; laid out contiguously where they hold plain elements.
    static constexpr std::array<tag, 3> gives_elements { layout::none, referencing::none, memory::retains };
    struct operation_costs
    {
        static constexpr cost insert = costs::lin_row;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr cost access = costs::one;
        static constexpr double words = 1.2;
    };
    static constexpr std::string_view description = "jagged array (CSR: offsets + one data array)";
    static constexpr std::string_view holds = "one offsets array per level";
};

/// jagged array of rows changed in place (fixed shape: offsets + one data array)
struct fixed_jagged_decl
{
    static constexpr std::string_view id = "fixed_jagged";
    static constexpr auto headers = header_list<1> { "urd/repr/sequences/jagged.hpp" };
    using kind = sequence_kind;
    static constexpr property_set provides = property_set::of({ position::positional, shape::fixed_shape, memory::retains, clearing::none });
    static constexpr role_tags relies_on = relies(stated({ shape::fixed_shape }), traits<role::rows, trait::rows_in_place>());
    /// Rows move with the data array and keep its memory; laid out contiguously where they are sequences of elements.
    static constexpr std::array<tag, 3> gives_elements { layout::none, referencing::none, memory::retains };
    struct operation_costs
    {
        static constexpr cost iterate = costs::lin_n;
        static constexpr cost access = costs::one;
        static constexpr double words = 1.2;
    };
    static constexpr std::string_view description = "jagged array of rows changed in place (fixed shape: offsets + one data array)";
    static constexpr std::string_view holds = "one offsets array per level";
};
template<typename Inner>
struct declaration_of<repr::Jagged<Inner>>
{
    using declaration = std::conditional_t<rows_flatten<Inner>(), jagged_decl, fixed_jagged_decl>;
    static constexpr role_tags roles = element_roles<Inner>();
};

}  // namespace detail

namespace detail
{

/// row pool (cleared rows are kept for reuse)
struct row_pool_decl
{
    static constexpr std::string_view id = "row_pool";
    static constexpr auto headers = header_list<1> { "urd/repr/sequences/row_pool.hpp" };
    using kind = sequence_kind;
    static constexpr property_set provides = property_set::of({ position::positional, layout::contiguous, memory::retains, clearing::proportional_clear });
    static constexpr role_tags relies_on = relies(traits<role::rows, trait::rows_reused>());
    static constexpr std::array<tag, 0> gives_elements {};
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr cost access = costs::one;
        static constexpr double words = 4;
    };
    static constexpr std::string_view description = "row pool (cleared rows are kept for reuse)";
    static constexpr std::string_view holds = "rows kept with their storage";
};
template<typename Inner>
struct declaration_of<repr::RowPool<Inner>>
{
    using declaration = row_pool_decl;
    static constexpr role_tags roles = element_roles<Inner>();
};

}  // namespace detail

namespace detail
{

/// jagged rows that track the rows handed out to change
struct tracked_jagged_decl
{
    static constexpr std::string_view id = "tracked_jagged";
    static constexpr auto headers = header_list<1> { "urd/repr/sequences/jagged.hpp" };
    using kind = sequence_kind;
    static constexpr property_set provides = property_set::of({ position::positional, shape::fixed_shape, memory::retains, clearing::proportional_clear });
    static constexpr role_tags relies_on = relies(stated({ shape::fixed_shape }), traits<role::rows, trait::rows_in_place>());
    /// Rows move with the data array and keep its memory; laid out contiguously where they are sequences of elements.
    static constexpr std::array<tag, 3> gives_elements { layout::none, referencing::none, memory::retains };
    struct operation_costs
    {
        static constexpr cost iterate = costs::lin_n;
        static constexpr cost access = costs::one;
        static constexpr double words = 1.3;
    };
    static constexpr std::string_view description = "jagged rows that track the rows handed out to change";
    static constexpr std::string_view holds = "one array, + the rows handed out to change";
};
template<typename Inner>
struct declaration_of<repr::Jagged<Inner, true>>
{
    using declaration = tracked_jagged_decl;
    static constexpr role_tags roles = element_roles<Inner>();
};

}  // namespace detail

}  // namespace urd
