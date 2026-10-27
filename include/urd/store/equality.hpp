#pragma once

#include "urd/store/types.hpp"

#include <algorithm>
#include <concepts>
#include <ranges>

namespace urd::detail::facet
{

/// Comparable for equal contents: by the representation's ==, else element by element where the Store is iterable.
template<typename Derived, typename R, typename S>
class equality
{
public:
    [[nodiscard]] friend bool operator==(const Derived& lhs, const Derived& rhs)
    {
        if constexpr (std::equality_comparable<R>)
            return store_access::representation(lhs) == store_access::representation(rhs);
        else
            return std::ranges::equal(lhs, rhs);
    }
};

}  // namespace urd::detail::facet
