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

#include <algorithm>
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

/// @brief `EqualTo` is our custom equality comparator, like std::equal_to.
///
/// Forwards to std::equal_to by default.
/// Specializations can be injected into the namespace.
template<typename T>
struct EqualTo
{
    constexpr bool operator()(const T& lhs, const T& rhs) const noexcept(noexcept(std::equal_to<T> {}(lhs, rhs)))
        requires requires {
            { lhs == rhs } -> std::convertible_to<bool>;
        }
    { return std::equal_to<T> {}(lhs, rhs); }
};

template<std::floating_point T>
struct EqualTo<T>
{
    constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
    {
        const bool lhs_nan = urd::is_nan(lhs);
        const bool rhs_nan = urd::is_nan(rhs);

        if (lhs_nan || rhs_nan)
            return lhs_nan && rhs_nan;

        return std::equal_to<T> {}(lhs, rhs);
    }
};

/// A value range equals another holding equal elements in the same order.
template<typename R>
    requires value_range<R>
struct EqualTo<R>
{
    constexpr bool operator()(const R& lhs, const R& rhs) const
        requires EqualityComparableByEqualTo<std::ranges::range_value_t<R>>
    { return std::ranges::equal(lhs, rhs, EqualTo<std::ranges::range_value_t<R>> {}); }
};

template<typename T>
struct EqualTo<std::reference_wrapper<T>>
{
    constexpr bool operator()(const std::reference_wrapper<T>& lhs, const std::reference_wrapper<T>& rhs) const
        requires EqualityComparableByEqualTo<T>
    { return EqualTo<std::remove_cvref_t<T>> {}(lhs.get(), rhs.get()); }
};

template<typename... Ts>
struct EqualTo<std::tuple<Ts...>>
{
    constexpr bool operator()(const std::tuple<Ts...>& lhs, const std::tuple<Ts...>& rhs) const
        requires(EqualityComparableByEqualTo<Ts> && ...)
    {
        return [&]<std::size_t... I>(std::index_sequence<I...> /*members*/)
        { return (EqualTo<std::remove_cvref_t<Ts>> {}(std::get<I>(lhs), std::get<I>(rhs)) && ...); }(std::index_sequence_for<Ts...> {});
    }
};

/// A pair equals as the tuple of its two members.
/// A pair (std's or cista's), as the tuple of its two members.
template<template<typename, typename> typename P, typename T1, typename T2>
    requires pair_value<P<T1, T2>>
struct EqualTo<P<T1, T2>>
{
    constexpr bool operator()(const P<T1, T2>& lhs, const P<T1, T2>& rhs) const
        requires EqualityComparableByEqualTo<T1> && EqualityComparableByEqualTo<T2>
    { return EqualTo<std::tuple<const T1&, const T2&>> {}(std::tie(lhs.first, lhs.second), std::tie(rhs.first, rhs.second)); }
};

/// Variants (std's or cista's) are equal holding the same alternative, equal, or both nothing.
template<template<typename...> typename V, typename... Ts>
    requires variant_value<V<Ts...>>
struct EqualTo<V<Ts...>>
{
    constexpr bool operator()(const V<Ts...>& lhs, const V<Ts...>& rhs) const
        requires(EqualityComparableByEqualTo<Ts> && ...)
    {
        if (lhs.index() != rhs.index())
            return false;
        return lhs.index() >= sizeof...(Ts) || visit(
                   []<typename L, typename R>(const L& l, const R& r)
                   {
                       if constexpr (std::is_same_v<L, R>)
                           return EqualTo<L> {}(l, r);
                       else
                           return false;
                   },
                   lhs,
                   rhs);
    }
};
/// Optionals (std's or cista's) are equal both empty, or holding equal values.
template<optional_value M>
struct EqualTo<M>
{
    constexpr bool operator()(const M& lhs, const M& rhs) const
        requires EqualityComparableByEqualTo<optional_value_t<M>>
    {
        if (lhs.has_value() != rhs.has_value())
            return false;
        return !lhs.has_value() || EqualTo<optional_value_t<M>> {}(*lhs, *rhs);
    }
};
/// An identified type equals as what identifies it (detail::identity); it also compares with what identifies it given
/// alone (detail::identity_t, or its parts by value), so that a lookup may take it in place of the element.
template<Identifiable T>
struct EqualTo<T>
{
    using is_transparent = void;

    constexpr bool operator()(const T& lhs, const T& rhs) const
        requires EqualityComparableByEqualTo<detail::identity_t<T>>
    { return EqualTo<detail::identity_t<T>> {}(detail::identity(lhs), detail::identity(rhs)); }
    template<identity_of<T> U>
    constexpr bool operator()(const T& lhs, const U& rhs) const
        requires EqualityComparableByEqualTo<detail::identity_t<T>>
    { return EqualTo<detail::identity_t<T>> {}(detail::identity(lhs), rhs); }
    template<identity_of<T> U>
    constexpr bool operator()(const U& lhs, const T& rhs) const
        requires EqualityComparableByEqualTo<detail::identity_t<T>>
    { return EqualTo<detail::identity_t<T>> {}(lhs, detail::identity(rhs)); }
    template<identity_of<T> U, identity_of<T> V>
    constexpr bool operator()(const U& lhs, const V& rhs) const
        requires EqualityComparableByEqualTo<detail::identity_t<T>>
    { return EqualTo<detail::identity_t<T>> {}(lhs, rhs); }
};

/// Compares any two values by the first one's EqualTo.
template<>
struct EqualTo<void>
{
    using is_transparent = void;

    template<typename T, typename U>
        requires EqualToFor<EqualTo<std::remove_cvref_t<T>>, T, U>
    constexpr bool operator()(const T& lhs, const U& rhs) const
    { return EqualTo<std::remove_cvref_t<T>> {}(lhs, rhs); }
};

/// Whether two ranges hold equal elements (by EqualTo), in order.
template<std::ranges::input_range LhsRange, std::ranges::input_range RhsRange>
    requires EqualityComparableByEqualTo<std::ranges::range_value_t<LhsRange>>
constexpr bool equal_range(LhsRange&& lhs, RhsRange&& rhs)
{
    using Value = std::remove_cvref_t<std::ranges::range_value_t<LhsRange>>;
    return std::ranges::equal(lhs, rhs, EqualTo<Value> {});
}

}  // namespace urd
