#pragma once

#include "urd/store/types.hpp"

#include <algorithm>
#include <compare>

namespace urd::detail::facet
{

/// Iterated in an order the request asked for: compared in that order, as the standard's ordered containers are.
template<typename Derived, typename R, typename S>
class ordering
{
public:
    [[nodiscard]] friend auto operator<=>(const Derived& lhs, const Derived& rhs)
    { return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end()); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S and its representation R.
template<>
struct facet_condition<facet_id::ordering>
{
    static constexpr bool reads_container = true;
    template<typename R, typename S>
    static constexpr bool on()
    { return compares_in_order<R, S>(); }
};

}  // namespace urd::detail
