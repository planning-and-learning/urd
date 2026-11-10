#pragma once

#include "urd/elements/less.hpp"
#include "urd/store/types.hpp"

#include <algorithm>
#include <compare>
#include <concepts>
#include <ranges>
#include <type_traits>

namespace urd::detail::facet
{

/// Iterated in an order the request asked for: compared in that order, as the standard's ordered containers are, by
/// urd::Less on the elements, so that <=> agrees with == (urd::EqualTo: a NaN equals a NaN, and orders after numbers).
template<typename Derived, typename R, typename S>
class ordering
{
public:
    /// Where the elements its representation yields order by urd::Less (a flattened Store's rows are slices, which do
    /// not): asked where two Stores are compared, not where the Store is built (urd/store/equality.hpp).
    template<std::same_as<Derived> Self>
        requires OrderedByLess<std::ranges::range_value_t<Self>>
    [[nodiscard]] friend auto operator<=>(const Self& lhs, const Self& rhs)
    { return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), ThreeWayCompare<> {}); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S and its representation R.
template<>
struct facet_condition<facet::ordering> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return !std::is_void_v<R> && store_types<S>::ordered; }  // R void: never asked from the spec alone
};

}  // namespace urd::detail
