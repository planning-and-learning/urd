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
