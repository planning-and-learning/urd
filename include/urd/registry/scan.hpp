#pragma once

// Choosing a container as constant evaluation: the entries of the request's kind's table (urd/registry/table.hpp)
// whose tags match the request's, the first of them chosen. Matching reads only tags: the container provides what the
// request requires (satisfies), the request's roles promise what the container relies on and demands (promises), and
// nested Stores get what they ask for. Where the request holds a tag that puts some entries later (urd::tag_defaults,
// later), they come after the others.

#include "urd/registry/table.hpp"

#include <cstddef>

namespace urd::detail
{

/// What a request is, as the scan reads it: what it requires of a container, its roles (what it states and what its
/// data is: urd/registry/facts.hpp), and where the elements it holds (a map's values, else its elements) are Stores,
/// their tags and what they require; else the roots.
struct request_shape
{
    property_set required;
    property_set roles;  ///< its facts: what it states, and what its data is, by role
    property_set held = property_set::roots();
    property_set held_required = property_set::roots();
};

/// Whether entry `e` serves request `s`: it provides what is required, the request promises what it relies on and
/// demands, and Stores held as elements get what they require.
constexpr bool serves(const registry_entry& e, const request_shape& s)
{
    if (!satisfies(e.provides, s.required) || !promises(s.roles, e.relies_on) || !promises(s.roles, demands_of(e.provides)))
        return false;
    return satisfies(with(s.held, e.gives_elements), s.held_required);
}

/// Whether entry `e` comes after the others serving request `s`: it holds a tag one of the request's tags puts later
/// (urd::tag_defaults, later: frozen and small put stable_refs later).
constexpr bool comes_later(const registry_entry& e, const request_shape& s)
{
    auto result = false;
    s.roles.for_each_listed<&category_table::later>([&](tag later) { result = result || e.provides.has(later); });
    return result;
}

/// No entry: the request is served by none.
inline constexpr std::size_t no_entry = static_cast<std::size_t>(-1);

/// The entries of Kind serving a request, by position in its table, in the order they are preferred.
template<typename Kind>
using serving_entries = fixed_list<std::size_t, table_of<Kind>.size()>;
template<typename Kind>
constexpr serving_entries<Kind> all_serving(const request_shape& s)
{
    auto result = serving_entries<Kind> {};
    auto later = serving_entries<Kind> {};
    for (std::size_t each = 0; each < table_of<Kind>.size(); ++each)
        if (serves(table_of<Kind>[each], s))
            (comes_later(table_of<Kind>[each], s) ? later : result).push(each);
    for (const auto each : later)
        result.push(each);
    return result;
}

}  // namespace urd::detail
