#pragma once

#include "urd/store/store_facts.hpp"
#include "urd/store/types.hpp"

#include <concepts>
#include <type_traits>

namespace urd::detail::facet
{

/// Comparable for equal contents, as its container compares them (urd::EqualTo), where it does: asked where two
/// Stores are compared, not where the Store is built, when its element type may still be incomplete. A template over
/// the Store itself, so that a lookup that only finds it (an iterator over Stores compared) never asks.
template<typename Derived, typename R, typename S>
class equality
{
public:
    template<std::same_as<Derived> Self>
        requires EqualityComparableByEqualTo<R>
    [[nodiscard]] friend bool operator==(const Self& lhs, const Self& rhs)
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
    { return !std::is_void_v<R> && !has<S, layered>; }  // R void: asked from the spec alone, before a container is chosen
};

}  // namespace urd::detail
