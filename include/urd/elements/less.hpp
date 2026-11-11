/*
 * Copyright (C) 2025-2026 Dominik Drexler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "urd/elements/concepts.hpp"
#include "urd/elements/equal_to.hpp"

#include <algorithm>
#include <compare>
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

/// @brief `Less` is our custom less-than comparator, like std::less.
///
/// Forwards to std::less by default.
/// Specializations can be injected into the namespace.
/// Every specialization must define a total order whose equivalence agrees
/// with `EqualTo`.
template<typename T>
struct Less
{
    constexpr bool operator()(const T& lhs, const T& rhs) const noexcept(noexcept(std::less<T> {}(lhs, rhs)))
        requires requires {
            { lhs < rhs } -> std::convertible_to<bool>;
        }
    { return std::less<T> {}(lhs, rhs); }
};

template<std::floating_point T>
struct Less<T>
{
    constexpr bool operator()(T lhs, T rhs) const noexcept
    {
        const bool lhs_nan = urd::is_nan(lhs);
        const bool rhs_nan = urd::is_nan(rhs);

        if (lhs_nan || rhs_nan)
            return !lhs_nan && rhs_nan;

        return std::less<T> {}(lhs, rhs);
    }
};

template<typename... Ts>
struct Less<std::tuple<Ts...>>
{
    constexpr bool operator()(const std::tuple<Ts...>& lhs, const std::tuple<Ts...>& rhs) const
        requires(OrderedByLess<Ts> && ...)
    { return less_from<0>(lhs, rhs); }

private:
    template<std::size_t I>
    static constexpr bool less_from(const std::tuple<Ts...>& lhs, const std::tuple<Ts...>& rhs)
    {
        if constexpr (I == sizeof...(Ts))
            return false;
        else
        {
            using Element = std::remove_cvref_t<std::tuple_element_t<I, std::tuple<Ts...>>>;
            if (Less<Element> {}(std::get<I>(lhs), std::get<I>(rhs)))
                return true;
            if (Less<Element> {}(std::get<I>(rhs), std::get<I>(lhs)))
                return false;
            return less_from<I + 1>(lhs, rhs);
        }
    }
};

/// A pair orders as the tuple of its two members.
/// A pair (std's or cista's), as the tuple of its two members.
template<template<typename, typename> typename P, typename T1, typename T2>
    requires pair_value<P<T1, T2>>
struct Less<P<T1, T2>>
{
    constexpr bool operator()(const P<T1, T2>& lhs, const P<T1, T2>& rhs) const
        requires OrderedByLess<T1> && OrderedByLess<T2>
    { return Less<std::tuple<const T1&, const T2&>> {}(std::tie(lhs.first, lhs.second), std::tie(rhs.first, rhs.second)); }
};

/// A value range orders as its elements, lexicographically.
template<typename R>
    requires value_range<R>
struct Less<R>
{
    constexpr bool operator()(const R& lhs, const R& rhs) const
        requires OrderedByLess<std::ranges::range_value_t<R>>
    { return std::ranges::lexicographical_compare(lhs, rhs, Less<std::ranges::range_value_t<R>> {}); }
};

template<typename T>
struct Less<std::reference_wrapper<T>>
{
    constexpr bool operator()(const std::reference_wrapper<T>& lhs, const std::reference_wrapper<T>& rhs) const
        noexcept(noexcept(Less<std::remove_cvref_t<T>> {}(lhs.get(), rhs.get())))
        requires OrderedByLess<T>
    { return Less<std::remove_cvref_t<T>> {}(lhs.get(), rhs.get()); }
};

/// Optionals (std's or cista's) order the empty one first, then by value.
template<optional_value M>
struct Less<M>
{
    constexpr bool operator()(const M& lhs, const M& rhs) const
        requires OrderedByLess<optional_value_t<M>>
    {
        if (lhs.has_value() != rhs.has_value())
            return !lhs.has_value();
        return lhs.has_value() && Less<optional_value_t<M>> {}(*lhs, *rhs);
    }
};
/// Variants (std's or cista's) order by the index they hold, then by the alternative.
template<template<typename...> typename V, typename... Ts>
    requires variant_value<V<Ts...>>
struct Less<V<Ts...>>
{
    constexpr bool operator()(const V<Ts...>& lhs, const V<Ts...>& rhs) const
        requires(OrderedByLess<Ts> && ...)
    {
        if (lhs.index() != rhs.index())
            return lhs.index() < rhs.index();
        return lhs.index() < sizeof...(Ts)
               && visit(
                   []<typename L, typename R>(const L& l, const R& r)
                   {
                       if constexpr (std::is_same_v<L, R>)
                           return Less<L> {}(l, r);
                       else
                           return false;
                   },
                   lhs,
                   rhs);
    }
};
/// An identified type orders as what identifies it (detail::identity); it also orders with what identifies it given
/// alone (detail::identity_t, or its parts by value), so that a lookup may take it in place of the element.
template<Identifiable T>
struct Less<T>
{
    using is_transparent = void;

    constexpr bool operator()(const T& lhs, const T& rhs) const
        requires OrderedByLess<detail::identity_t<T>>
    { return Less<detail::identity_t<T>> {}(detail::identity(lhs), detail::identity(rhs)); }
    template<identity_of<T> U>
    constexpr bool operator()(const T& lhs, const U& rhs) const
        requires OrderedByLess<detail::identity_t<T>>
    { return Less<detail::identity_t<T>> {}(detail::identity(lhs), rhs); }
    template<identity_of<T> U>
    constexpr bool operator()(const U& lhs, const T& rhs) const
        requires OrderedByLess<detail::identity_t<T>>
    { return Less<detail::identity_t<T>> {}(lhs, detail::identity(rhs)); }
    template<identity_of<T> U, identity_of<T> V>
    constexpr bool operator()(const U& lhs, const V& rhs) const
        requires OrderedByLess<detail::identity_t<T>>
    { return Less<detail::identity_t<T>> {}(lhs, rhs); }
};

/// Orders any two values by the first one's Less.
template<>
struct Less<void>
{
    using is_transparent = void;

    template<typename T, typename U>
        requires LessFor<Less<std::remove_cvref_t<T>>, T, U>
    constexpr bool operator()(const T& lhs, const U& rhs) const noexcept(noexcept(Less<std::remove_cvref_t<T>> {}(lhs, rhs)))
    { return Less<std::remove_cvref_t<T>> {}(lhs, rhs); }
};

namespace detail
{
/// The Less a predicate of T uses on a U: T's, or with T void (transparent), U's.
template<typename T, typename U>
using order_of = Less<std::conditional_t<std::is_void_v<T>, std::remove_cvref_t<U>, T>>;
}  // namespace detail

/// The other order predicates and a three-way comparison, all from Less, so that every predicate shares its order.
template<typename T = void>
struct LessEqual
{
    using is_transparent = void;

    template<typename U = T, typename V = T>
        requires LessFor<detail::order_of<T, U>, V, U>
    constexpr bool operator()(const U& lhs, const V& rhs) const
    { return !detail::order_of<T, U> {}(rhs, lhs); }
};
template<typename T = void>
struct Greater
{
    using is_transparent = void;

    template<typename U = T, typename V = T>
        requires LessFor<detail::order_of<T, U>, V, U>
    constexpr bool operator()(const U& lhs, const V& rhs) const
    { return detail::order_of<T, U> {}(rhs, lhs); }
};
template<typename T = void>
struct GreaterEqual
{
    using is_transparent = void;

    template<typename U = T, typename V = T>
        requires LessFor<detail::order_of<T, U>, U, V>
    constexpr bool operator()(const U& lhs, const V& rhs) const
    { return !detail::order_of<T, U> {}(lhs, rhs); }
};
template<typename T = void>
struct ThreeWayCompare
{
    using is_transparent = void;

    template<typename U = T, typename V = T>
        requires LessFor<detail::order_of<T, U>, U, V> && LessFor<detail::order_of<T, U>, V, U>
    constexpr std::strong_ordering operator()(const U& lhs, const V& rhs) const
    {
        using Compare = detail::order_of<T, U>;
        if (Compare {}(lhs, rhs))
            return std::strong_ordering::less;
        if (Compare {}(rhs, lhs))
            return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }
};

/// Whether `lhs` comes before `rhs` element by element (by Less), as std::lexicographical_compare.
template<std::ranges::input_range LhsRange, std::ranges::input_range RhsRange>
    requires OrderedByLess<std::ranges::range_value_t<LhsRange>>
constexpr bool less_range(LhsRange&& lhs, RhsRange&& rhs)
{
    using Value = std::remove_cvref_t<std::ranges::range_value_t<LhsRange>>;
    return std::ranges::lexicographical_compare(std::forward<LhsRange>(lhs), std::forward<RhsRange>(rhs), Less<Value> {});
}

/// Gives Derived `==` and `<=>` by what identifies it (its identifying_members(), through EqualTo and ThreeWayCompare):
/// `struct Key : urd::compared_by_identity<Key> { … identifying_members() … };`.
template<typename Derived>
struct compared_by_identity
{
    friend constexpr bool operator==(const Derived& lhs, const Derived& rhs)
        requires Identifiable<Derived>
    { return EqualTo<Derived> {}(lhs, rhs); }

    friend constexpr std::strong_ordering operator<=>(const Derived& lhs, const Derived& rhs)
        requires Identifiable<Derived> && std::invocable<ThreeWayCompare<Derived>, const Derived&, const Derived&>
    { return ThreeWayCompare<Derived> {}(lhs, rhs); }
};

}  // namespace urd
