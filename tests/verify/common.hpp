#pragma once

// What every kind's check reads: the contents a range holds, element addresses, positions, and what an insert reports.

#include "urd/semantics/less.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::detail
{

/// Whether the contents of `range` are as Traits says: sorted, unique, nonempty. The other properties are
/// about the type, not the contents; the checks below cover those. O(n log n) at most.
template<typename Traits, std::ranges::forward_range R>
[[nodiscard]] bool holds(const R& range)
{
    if (has<Traits, nonempty> && std::ranges::empty(range))
        return false;
    if (has<Traits, sorted> && !std::ranges::is_sorted(range))
        return false;
    if constexpr (has<Traits, unique>)
    {
        if constexpr (has<Traits, sorted>)
            return std::ranges::adjacent_find(range) == std::ranges::end(range);
        auto copy = std::vector<std::ranges::range_value_t<R>>(std::ranges::begin(range), std::ranges::end(range));
        std::ranges::sort(copy);
        return std::ranges::adjacent_find(copy) == copy.end();
    }
    return true;
}

template<typename R>
using element_t = std::ranges::range_value_t<R>;

/// A map: it names what its keys map to.
template<typename R>
concept map_container = requires {
    typename R::mapped_type;
    requires !std::is_void_v<typename R::mapped_type>;
};

template<typename R>
std::vector<const element_t<R>*> addresses(const R& range)
{
    auto result = std::vector<const element_t<R>*>();
    for (const auto& element : range)
        result.push_back(std::addressof(element));
    return result;
}

/// The addresses of a map's values: what a reference to a mapped value points at, taken through the entries
/// iteration gives (a dense map builds its entry on the fly; its .second refers to the stored value).
template<typename R>
std::vector<const typename R::mapped_type*> value_addresses(const R& range)
{
    auto result = std::vector<const typename R::mapped_type*>();
    for (const auto& entry : range)
        result.push_back(std::addressof(entry.second));
    return result;
}

template<typename R>
std::ranges::iterator_t<const R> const_at(const R& range, std::size_t pos)
{ return std::ranges::next(std::ranges::begin(range), static_cast<std::ranges::range_difference_t<R>>(pos)); }

template<typename R>
std::size_t position_of(const R& range, const std::ranges::iterator_t<const R>& it)
{ return static_cast<std::size_t>(std::ranges::distance(std::ranges::begin(range), it)); }

/// Whether an insert's result says what the model's insert did: for a unique container, whether it inserted;
/// for any, an iterator to an element equal to `element`.
template<typename Result, typename E>
bool reports(const Result& result, bool inserted, const E& element)
{
    if constexpr (requires { result.second; })
        return result.second == inserted && *result.first == element;
    else
        return inserted && *result == element;
}

}  // namespace urd::detail
