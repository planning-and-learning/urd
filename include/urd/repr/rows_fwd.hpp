#pragma once

// Representations of nested Stores, whose elements (rows) are Stores themselves, declared with what the registry
// reads of them. Defined in urd/repr/jagged.hpp and urd/repr/row_pool.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/interface.hpp"
#include "urd/registry/element_concepts.hpp"
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

/// Rows in one array: appends at the end, and erase and clear keep the storage at every level.
template<detail::flattenable Inner>
struct container_traits<repr::Jagged<Inner>> : properties_of<dense, positional, shrinkable, retains, proportional_clear>
{
};

/// The rows are frozen, their elements move when rows before them are inserted or erased, and their storage is
/// kept; innermost rows lie next to each other.
template<detail::flattenable Inner>
struct element_traits<repr::Jagged<Inner>> :
    with_t<container_traits<Inner>, std::conditional_t<is_store<typename container_traits<Inner>::element_type>, any_layout, contiguous>, moving_refs, retains>
{
};

/// Rows of fixed lengths, changed in place; with TrackRows, reset costs what the rows handed out since hold.
template<detail::fixed_rows Inner, bool TrackRows>
struct container_traits<repr::Jagged<Inner, TrackRows>> :
    properties_of<dense, positional, fixed_shape, retains, std::conditional_t<TrackRows, proportional_clear, any_clear>>
{
};

/// The rows keep their lengths and their storage; a sequence's elements lie next to each other.
template<detail::fixed_rows Inner, bool TrackRows>
struct element_traits<repr::Jagged<Inner, TrackRows>> :
    with_t<container_traits<Inner>,
           std::conditional_t<!is_store<typename container_traits<Inner>::element_type> && !by_value<container_traits<Inner>>, contiguous, any_layout>,
           moving_refs,
           retains>
{
};

/// Rows that can be cleared are kept for reuse.
template<typename Inner>
    requires(is_store<Inner> && has<container_traits<Inner>, shrinkable>)
struct container_traits<repr::RowPool<Inner>> : properties_of<dense, positional, contiguous, shrinkable, retains, proportional_clear>
{
};

/// The rows keep what they hold when removed: their own properties, memory included.
template<typename Inner>
    requires(is_store<Inner> && has<container_traits<Inner>, shrinkable>)
struct element_traits<repr::RowPool<Inner>> : container_traits<Inner>
{
};

template<typename Inner, bool TrackRows>
inline constexpr auto defined_in<repr::Jagged<Inner, TrackRows>> = detail::fixed_string("urd/repr/jagged.hpp");
template<typename Inner>
inline constexpr auto defined_in<repr::RowPool<Inner>> = detail::fixed_string("urd/repr/row_pool.hpp");

}  // namespace urd

namespace urd
{

/// jagged.
template<typename Inner>
struct container_costs<repr::Jagged<Inner>>
{
    static constexpr cost insert = costs::lin_row_t;
    static constexpr cost erase = costs::lin_nt;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 1.2;
};

/// row_pool.
template<typename Inner>
struct container_costs<repr::RowPool<Inner>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost erase = costs::one_t;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 4;
};

/// tracked_jagged.
template<typename Inner>
struct container_costs<repr::Jagged<Inner, true>>
{
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 1.3;
};

}  // namespace urd

namespace urd
{

template<typename Inner>
struct container_synopsis<repr::Jagged<Inner>>
{
    static constexpr std::string_view id = "jagged";
    static constexpr std::string_view description = "jagged array (CSR: offsets + one data array)";
    static constexpr std::string_view holds = "one offsets array per level";
};

template<typename Inner>
struct container_synopsis<repr::RowPool<Inner>>
{
    static constexpr std::string_view id = "row_pool";
    static constexpr std::string_view description = "row pool (cleared rows are kept for reuse)";
    static constexpr std::string_view holds = "rows kept with their storage";
};

template<typename Inner>
struct container_synopsis<repr::Jagged<Inner, true>>
{
    static constexpr std::string_view id = "tracked_jagged";
    static constexpr std::string_view description = "jagged rows that track the rows handed out to change";
    static constexpr std::string_view holds = "one array, + the rows handed out to change";
};

}  // namespace urd
