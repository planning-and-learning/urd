#pragma once

#include "urd/props.hpp"
#include "urd/refined.hpp"

#include <algorithm>
#include <concepts>
#include <iterator>
#include <ranges>
#include <utility>
#include <vector>

namespace urd
{

/// How contains() looks up a value in a range of type R.
enum class lookup
{
    member,         ///< the container's own lookup (std::set::contains, a hash lookup)
    binary_search,  ///< the range is sorted and random access: O(log n)
    linear_scan,    ///< nothing better is known: O(n)
};

template<std::ranges::forward_range R>
[[nodiscard]] consteval lookup lookup_of()
{
    if constexpr (requires(const R& range, const std::ranges::range_value_t<R>& value) {
                      { range.contains(value) } -> std::convertible_to<bool>;
                  })
        return lookup::member;
    else if constexpr (has_props<R, props::sorted> && std::ranges::random_access_range<R>)
        return lookup::binary_search;
    else
        return lookup::linear_scan;
}

/// Whether `range` contains `value`, by the best lookup its type allows (lookup_of).
template<std::ranges::forward_range R>
[[nodiscard]] bool contains(const R& range, const std::ranges::range_value_t<R>& value)
{
    if constexpr (lookup_of<R>() == lookup::member)
        return range.contains(value);
    else if constexpr (lookup_of<R>() == lookup::binary_search)
        return std::ranges::binary_search(range, value);
    else
        return std::ranges::find(range, value) != std::ranges::end(range);
}

/// Two sorted, unique ranges of the same element type: sets that can be merged in linear time.
template<typename A, typename B>
concept mergeable_sets = has_props<A, props::sorted | props::unique> && has_props<B, props::sorted | props::unique>
    && std::same_as<std::ranges::range_value_t<A>, std::ranges::range_value_t<B>>;

/// The elements in both `a` and `b`, in O(|a| + |b|). The result is sorted and unique; it may be
/// empty even when both inputs are not.
template<std::ranges::forward_range A, std::ranges::forward_range B>
    requires mergeable_sets<A, B>
[[nodiscard]] SortedUniqueVector<std::ranges::range_value_t<A>> intersect(const A& a, const B& b)
{
    auto out = std::vector<std::ranges::range_value_t<A>>();
    std::ranges::set_intersection(a, b, std::back_inserter(out));
    return SortedUniqueVector<std::ranges::range_value_t<A>>::assume(std::move(out));
}

/// The properties of unite(a, b): sorted and unique, and nonempty when either input is.
template<typename A, typename B>
inline constexpr props union_props = props::sorted | props::unique
    | (includes(properties_of_v<A>, props::nonempty) || includes(properties_of_v<B>, props::nonempty) ? props::nonempty
                                                                                                      : props::none);

/// The elements in `a` or `b`, in O(|a| + |b|).
template<std::ranges::forward_range A, std::ranges::forward_range B>
    requires mergeable_sets<A, B>
[[nodiscard]] Refined<std::vector<std::ranges::range_value_t<A>>, union_props<A, B>> unite(const A& a, const B& b)
{
    auto out = std::vector<std::ranges::range_value_t<A>>();
    std::ranges::set_union(a, b, std::back_inserter(out));
    return Refined<std::vector<std::ranges::range_value_t<A>>, union_props<A, B>>::assume(std::move(out));
}

}  // namespace urd
