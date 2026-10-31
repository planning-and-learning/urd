#pragma once

// Choosing a container as constant evaluation: each entry of the table (urd/registry/table.hpp) is asked for its
// offer for the request's elements, and the first whose offer serves the request is chosen. Serving reads only what
// the offer and the request declare: the kinds match (holds_kind), the container gives what the request requires
// (satisfies), the request promises what the container relies on and demands (promises), and nested Stores get what
// they ask for. Where the request holds a tag that prefers some entries later (urd/tags.hpp, preferences), they come
// after the others.

#include "urd/registry/table.hpp"

#include <cstddef>

namespace urd::detail
{

/// What a request is, as the scan reads it: its kind, what it promises its container, and what a container must
/// guarantee for it.
struct request_shape
{
    urd::kind kind = urd::kind::sequence;
    property_set promised;
    property_set required;
};

/// Whether offer `o` serves a request of shape `s` with facts `f`.
constexpr bool serves(const offer& o, const request_shape& s, const request_features& f)
{
    if (!holds_kind(o.kind, s.kind) || !satisfies(o.properties, s.required) || !promises(s.promised, o.relies_on)
        || !promises(s.promised, demands_of(o.properties)))
        return false;
    const auto& held = s.kind == urd::kind::map ? f.mapped : f.element;
    return !held.store || satisfies(o.nested, held.store->required);
}

/// Whether offer `o` comes after the others serving a request of shape `s`, by the preferences (urd/tags.hpp).
constexpr bool comes_later(const offer& o, const request_shape& s)
{
    for (const auto& each : preferences)
        if (s.promised.has(each.when) && o.properties.has(each.later))
            return true;
    return false;
}

/// No entry: the request is served by none.
inline constexpr std::size_t no_entry = static_cast<std::size_t>(-1);

/// The entries serving a request, by position in the table, in the order they are preferred.
struct serving_entries
{
    std::array<std::size_t, registry_table.size()> at {};
    std::size_t count = 0;
};
constexpr serving_entries all_serving(const request_shape& s, const request_features& f)
{
    auto result = serving_entries {};
    auto later = serving_entries {};
    for (std::size_t each = 0; each < registry_table.size(); ++each)
    {
        const auto o = registry_table[each].offers(f);
        if (!o || !serves(*o, s, f))
            continue;
        if (comes_later(*o, s))
            later.at[later.count++] = each;
        else
            result.at[result.count++] = each;
    }
    for (std::size_t each = 0; each < later.count; ++each)
        result.at[result.count++] = later.at[each];
    return result;
}

/// The position in the table of the entry chosen for the request, or no_entry: the first of all_serving, found
/// without listing the rest.
constexpr std::size_t first_serving(const request_shape& s, const request_features& f)
{
    auto later = no_entry;
    for (std::size_t each = 0; each < registry_table.size(); ++each)
    {
        const auto o = registry_table[each].offers(f);
        if (!o || !serves(*o, s, f))
            continue;
        if (!comes_later(*o, s))
            return each;
        if (later == no_entry)
            later = each;
    }
    return later;
}

}  // namespace urd::detail
