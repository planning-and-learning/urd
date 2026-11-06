#pragma once

#include "urd/store/store_facts.hpp"
#include "urd/store/types.hpp"

#include <type_traits>

namespace urd::detail::facet
{

/// Comparable for equal contents, as its container compares them (urd::EqualTo).
template<typename Derived, typename R, typename S>
class equality
{
public:
    [[nodiscard]] friend bool operator==(const Derived& lhs, const Derived& rhs)
    { return EqualTo<R> {}(store_access::representation(lhs), store_access::representation(rhs)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S and its representation R.
/// A layered Store's contents include its parent's, which neither its == nor its iteration covers.
template<>
struct facet_condition<facet::equality> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    {
        if constexpr (std::is_void_v<R>)  // asked from the spec alone, before a container is chosen
            return false;
        else
            return EqualityComparableByEqualTo<R> && !has<S, layered>;
    }
};

}  // namespace urd::detail
