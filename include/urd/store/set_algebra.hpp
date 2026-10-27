#pragma once

#include "urd/store/types.hpp"

#include <algorithm>
#include <ranges>

namespace urd::detail::facet
{

/// A set: inclusion and overlap with another set of its kind, and union, intersection and difference in place.
/// Each uses the representation's own operation where it has one (a bitset combines whole words), else lookups.
/// The ones that change the set need it growable (union) or shrinkable (intersection, difference).
template<typename Derived, typename R, typename S>
class set_algebra
{
public:
    /// Whether every element of `other` is in this set.
    [[nodiscard]] bool includes(const Derived& other) const
    {
        if constexpr (requires { representation().includes(store_access::representation(other)); })
            return representation().includes(store_access::representation(other));
        else
            return std::ranges::all_of(other, [this](const auto& element) { return self().contains(element); });
    }
    /// Whether this set and `other` share an element.
    [[nodiscard]] bool intersects(const Derived& other) const
    {
        if constexpr (requires { representation().intersects(store_access::representation(other)); })
            return representation().intersects(store_access::representation(other));
        else
            return std::ranges::any_of(other, [this](const auto& element) { return self().contains(element); });
    }

    Derived& operator|=(const Derived& other)
        requires has<S, growable>
    {
        if constexpr (requires { mutable_representation() |= store_access::representation(other); })
            mutable_representation() |= store_access::representation(other);
        else
            std::ranges::for_each(other, [this](const auto& element) { mutable_self().insert(element); });
        return mutable_self();
    }
    Derived& operator&=(const Derived& other)
        requires has<S, shrinkable>
    {
        if constexpr (requires { mutable_representation() &= store_access::representation(other); })
            mutable_representation() &= store_access::representation(other);
        else
            erase_if(mutable_self(), [&other](const auto& element) { return !other.contains(element); });
        return mutable_self();
    }
    Derived& operator-=(const Derived& other)
        requires has<S, shrinkable>
    {
        if constexpr (requires { mutable_representation() -= store_access::representation(other); })
            mutable_representation() -= store_access::representation(other);
        else
            erase_if(mutable_self(), [&other](const auto& element) { return other.contains(element); });
        return mutable_self();
    }

private:
    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
    [[nodiscard]] Derived& mutable_self() { return static_cast<Derived&>(*this); }
    [[nodiscard]] const R& representation() const { return store_access::representation(self()); }
    [[nodiscard]] R& mutable_representation() { return store_access::representation(mutable_self()); }
};

}  // namespace urd::detail::facet
