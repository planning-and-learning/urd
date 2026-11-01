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

// Copied from yggdrasil (include/yggdrasil/semantics/comparators.hpp); names moved from ygg to urd.

#ifndef URD_SEMANTICS_LESS_HPP_
#define URD_SEMANTICS_LESS_HPP_

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

/// Whether `lhs` comes before `rhs`, element by element by Less: two ranges of the same element type.
template<std::ranges::input_range Range>
    requires OrderedByLess<std::ranges::range_value_t<Range>>
constexpr bool ranges_less(const Range& lhs, const Range& rhs)
{ return std::ranges::lexicographical_compare(lhs, rhs, Less<std::ranges::range_value_t<Range>> {}); }

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
template<typename T1, typename T2>
struct Less<std::pair<T1, T2>>
{
    constexpr bool operator()(const std::pair<T1, T2>& lhs, const std::pair<T1, T2>& rhs) const
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
    { return ranges_less(lhs, rhs); }
};

template<typename T>
struct Less<std::reference_wrapper<T>>
{
    constexpr bool operator()(const std::reference_wrapper<T>& lhs, const std::reference_wrapper<T>& rhs) const
        noexcept(noexcept(Less<std::remove_cvref_t<T>> {}(lhs.get(), rhs.get())))
        requires OrderedByLess<T>
    { return Less<std::remove_cvref_t<T>> {}(lhs.get(), rhs.get()); }
};

template<typename T>
struct Less<std::optional<T>>
{
    constexpr bool operator()(const std::optional<T>& lhs, const std::optional<T>& rhs) const
        requires OrderedByLess<T>
    {
        if (lhs.has_value() != rhs.has_value())
            return !lhs.has_value();

        return lhs.has_value() ? Less<std::remove_cvref_t<T>> {}(lhs.value(), rhs.value()) : false;
    }
};

template<typename... Ts>
struct Less<std::variant<Ts...>>
{
    constexpr bool operator()(const std::variant<Ts...>& lhs, const std::variant<Ts...>& rhs) const
        requires(OrderedByLess<Ts> && ...)
    {
        if (lhs.index() != rhs.index())
            return lhs.index() < rhs.index();

        return std::visit(
            []<typename L, typename R>(const L& l, const R& r)
            {
                if constexpr (std::is_same_v<L, R>)
                    return Less<L> {}(l, r);
                return false;
            },
            lhs,
            rhs);
    }
};

/// An identified type orders as what identifies it (detail::identity).
template<Identifiable T>
struct Less<T>
{
    constexpr bool operator()(const T& lhs, const T& rhs) const
        requires OrderedByLess<detail::identity_t<T>>
    { return Less<detail::identity_t<T>> {}(detail::identity(lhs), detail::identity(rhs)); }
};

}  // namespace urd

#endif
