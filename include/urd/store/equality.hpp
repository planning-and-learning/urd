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

namespace urd::detail
{

/// When a Store has this facet: from its spec S and its representation R.
/// A layered Store's contents include its parent's, which neither its == nor its iteration covers.
template<>
struct facet_condition<facet_id::equality>
{
    static constexpr bool reads_container = true;
    template<typename R, typename S>
    static constexpr bool on()
    { return std::equality_comparable<typename S::element_type> && (std::equality_comparable<R> || S::iterable) && !has<S, layered>; }
};

}  // namespace urd::detail
