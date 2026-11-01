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

// Copied from yggdrasil (include/yggdrasil/semantics/equal_to.hpp); names moved from ygg to urd.

#ifndef URD_SEMANTICS_EQUAL_TO_HPP_
#define URD_SEMANTICS_EQUAL_TO_HPP_

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

/// Whether `lhs` and `rhs` hold equal elements, by EqualTo, in the same order: two ranges of the same element type.
template<std::ranges::input_range Range>
    requires EqualityComparableByEqualTo<std::ranges::range_value_t<Range>>
constexpr bool ranges_equal(const Range& lhs, const Range& rhs)
{ return std::ranges::equal(lhs, rhs, EqualTo<std::ranges::range_value_t<Range>> {}); }

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
    { return ranges_equal(lhs, rhs); }
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
template<typename T1, typename T2>
struct EqualTo<std::pair<T1, T2>>
{
    constexpr bool operator()(const std::pair<T1, T2>& lhs, const std::pair<T1, T2>& rhs) const
        requires EqualityComparableByEqualTo<T1> && EqualityComparableByEqualTo<T2>
    { return EqualTo<std::tuple<const T1&, const T2&>> {}(std::tie(lhs.first, lhs.second), std::tie(rhs.first, rhs.second)); }
};

template<typename... Ts>
struct EqualTo<std::variant<Ts...>>
{
    constexpr bool operator()(const std::variant<Ts...>& lhs, const std::variant<Ts...>& rhs) const
        requires(EqualityComparableByEqualTo<Ts> && ...)
    {
        if (lhs.index() != rhs.index())
            return false;

        return std::visit(
            []<typename L, typename R>(const L& l, const R& r)
            {
                if constexpr (std::is_same_v<L, R>)
                    return EqualTo<L> {}(l, r);
                return false;
            },
            lhs,
            rhs);
    }
};

template<typename T>
struct EqualTo<std::optional<T>>
{
    constexpr bool operator()(const std::optional<T>& lhs, const std::optional<T>& rhs) const
        requires EqualityComparableByEqualTo<T>
    {
        if (lhs.has_value() != rhs.has_value())
            return false;
        return !lhs.has_value() || EqualTo<std::remove_cvref_t<T>> {}(*lhs, *rhs);
    }
};

/// An identified type equals as what identifies it (detail::identity).
template<Identifiable T>
struct EqualTo<T>
{
    constexpr bool operator()(const T& lhs, const T& rhs) const
        requires EqualityComparableByEqualTo<detail::identity_t<T>>
    { return EqualTo<detail::identity_t<T>> {}(detail::identity(lhs), detail::identity(rhs)); }
};

}  // namespace urd

#endif
