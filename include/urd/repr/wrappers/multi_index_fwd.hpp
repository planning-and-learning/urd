#pragma once

// A multi-index pool: a pool of rows with hashed non-unique indexes on sets of columns (Boost.MultiIndex's
// hashed_non_unique), added at run time. Declared with what the registry reads of it; defined in
// urd/repr/wrappers/multi_index_pool.hpp. The pool it wraps is defined in its own header.

#include "urd/cost.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

namespace urd::repr
{

/// A pool of rows R and indexes on sets of its columns, each kept as rows are inserted and erased.
template<typename R>
class MultiIndexPool;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

inline constexpr std::string_view multi_indexed_prefix = "multi_indexed_";
inline constexpr std::string_view multi_indexed_description = "multi-index pool (hashed non-unique indexes on sets of columns) of a ";

/// Part's declaration with indexes on sets of columns: its id and words the part's, prefixed; its offer the part's
/// with multi_indexed. Over a pool of sequence rows that keeps them whole and numbers them (erase moves the last row).
template<typename Part>
struct multi_indexed_decl
{
    static constexpr std::string_view id = joined<multi_indexed_prefix, Part::id>::value;
    static constexpr auto headers = with_header(Part::headers, "urd/repr/wrappers/multi_index_pool.hpp");
    static_assert(Part::provides.has(uniqueness::unique) && Part::provides.has(handling::handles), "multi_indexed_decl: over a pool");
    using kind = typename Part::kind;
    static constexpr property_set provides = with(Part::provides, { indexing::multi_indexed, room::none });
    /// Its rows are sequences, whose columns it reads.
    static constexpr property_set relies_on = with(Part::relies_on, relies(tags<role::row>({ position::positional })));
    static constexpr auto gives_elements = Part::gives_elements;
    using operation_costs = costs_over<Part>;
    static constexpr double words = Part::words;
    static constexpr std::string_view description = joined<multi_indexed_description, Part::description>::value;
    static constexpr std::string_view holds = "its rows; per index a group of rows per key, linked through each row";
};

}  // namespace detail

template<typename R>
struct detail::declaration_of<repr::MultiIndexPool<R>>
{
    using declaration = multi_indexed_decl<declaration_t<R>>;
    static constexpr auto roles = declaration_of<R>::roles;
};

}  // namespace urd
