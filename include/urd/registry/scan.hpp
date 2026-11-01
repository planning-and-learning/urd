#pragma once

// Choosing a container as constant evaluation: the entries of the request's kind's table (urd/registry/table.hpp)
// whose tags match the request's, the first of them chosen. Matching reads only tags: the container provides what the
// request requires (satisfies), the request's roles promise what the container relies on and demands (promises), and
// nested Stores get what they ask for. Where the request holds a tag that prefers some entries later (urd/kinds.hpp, preferences), they come
// after the others.

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
    role_tags roles;
    property_set held = property_set::roots();
    property_set held_required = property_set::roots();
};

/// Whether entry `e` serves request `s`: it provides what is required, the request promises what it relies on and
/// demands, and Stores held as elements get what they require.
constexpr bool serves(const registry_entry& e, const request_shape& s)
{
    if (!satisfies(e.provides, s.required) || !promises(s.roles, e.relies_on) || !promises(s.roles.stated, demands_of(e.provides)))
        return false;
    return satisfies(with(s.held, e.gives_elements), s.held_required);
}

/// Whether entry `e` comes after the others serving request `s`, by the preferences (urd/kinds.hpp).
constexpr bool comes_later(const registry_entry& e, const request_shape& s)
{
    for (const auto& each : preferences)
        if (s.roles.stated.has(each.when) && e.provides.has(each.later))
            return true;
    return false;
}

/// No entry: the request is served by none.
inline constexpr std::size_t no_entry = static_cast<std::size_t>(-1);

/// The entries of Kind serving a request, by position in its table, in the order they are preferred.
template<typename Kind>
struct serving_entries
{
    std::array<std::size_t, table_of<Kind>.size()> at {};
    std::size_t count = 0;
};
template<typename Kind>
constexpr serving_entries<Kind> all_serving(const request_shape& s)
{
    auto result = serving_entries<Kind> {};
    auto later = serving_entries<Kind> {};
    for (std::size_t each = 0; each < table_of<Kind>.size(); ++each)
    {
        if (!serves(table_of<Kind>[each], s))
            continue;
        if (comes_later(table_of<Kind>[each], s))
            later.at[later.count++] = each;
        else
            result.at[result.count++] = each;
    }
    for (std::size_t each = 0; each < later.count; ++each)
        result.at[result.count++] = later.at[each];
    return result;
}

/// The position in Kind's table of the entry chosen for the request, or no_entry: the first of all_serving, found
/// without listing the rest.
template<typename Kind>
constexpr std::size_t first_serving(const request_shape& s)
{
    auto later = no_entry;
    for (std::size_t each = 0; each < table_of<Kind>.size(); ++each)
    {
        if (!serves(table_of<Kind>[each], s))
            continue;
        if (!comes_later(table_of<Kind>[each], s))
            return each;
        if (later == no_entry)
            later = each;
    }
    return later;
}

}  // namespace urd::detail
