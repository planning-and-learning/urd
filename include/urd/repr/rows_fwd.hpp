#pragma once

// Representations of nested Stores, whose elements (rows) are Stores themselves, declared with what the registry
// reads of them. Defined in urd/repr/jagged.hpp and urd/repr/row_pool.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/interface.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/element_concepts.hpp"
#include "urd/registry/features.hpp"
#include "urd/registry/guarantees.hpp"
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
    static constexpr auto headers = header_list<1> { "urd/repr/jagged.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::jagged(f.element, false);
        if (!held)
            return {};
        auto result = offer_of(urd::kind::sequence, *held, f);
        if (const auto nested = guarantees::jagged_rows(f.element, false))
            result = result.giving_nested(*nested);
        return result;
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::lin_row_t;
        static constexpr cost erase = costs::lin_nt;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 1.2;
    };
    static constexpr std::string_view description = "jagged array (CSR: offsets + one data array)";
    static constexpr std::string_view holds = "one offsets array per level";
};
template<typename Inner>
struct declaration_of<repr::Jagged<Inner>>
{
    using declaration = jagged_decl;
    static constexpr request_features features = element_facts<Inner>();
};

}  // namespace detail

namespace detail
{

/// row pool (cleared rows are kept for reuse)
struct row_pool_decl
{
    static constexpr std::string_view id = "row_pool";
    static constexpr auto headers = header_list<1> { "urd/repr/row_pool.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::row_pool(f.element);
        if (!held)
            return {};
        auto result = offer_of(urd::kind::sequence, *held, f);
        if (const auto nested = guarantees::own_rows(f.element))
            result = result.giving_nested(*nested);
        return result;
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_at;
        static constexpr cost erase = costs::one_t;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 4;
    };
    static constexpr std::string_view description = "row pool (cleared rows are kept for reuse)";
    static constexpr std::string_view holds = "rows kept with their storage";
};
template<typename Inner>
struct declaration_of<repr::RowPool<Inner>>
{
    using declaration = row_pool_decl;
    static constexpr request_features features = element_facts<Inner>();
};

}  // namespace detail

namespace detail
{

/// jagged rows that track the rows handed out to change
struct tracked_jagged_decl
{
    static constexpr std::string_view id = "tracked_jagged";
    static constexpr auto headers = header_list<1> { "urd/repr/jagged.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::jagged(f.element, true);
        if (!held)
            return {};
        auto result = offer_of(urd::kind::sequence, *held, f);
        if (const auto nested = guarantees::jagged_rows(f.element, true))
            result = result.giving_nested(*nested);
        return result;
    }
    struct operation_costs
    {
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 1.3;
    };
    static constexpr std::string_view description = "jagged rows that track the rows handed out to change";
    static constexpr std::string_view holds = "one array, + the rows handed out to change";
};
template<typename Inner>
struct declaration_of<repr::Jagged<Inner, true>>
{
    using declaration = tracked_jagged_decl;
    static constexpr request_features features = element_facts<Inner>();
};

}  // namespace detail

}  // namespace urd
