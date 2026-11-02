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

// Copied from yggdrasil (include/yggdrasil/core/concepts.hpp); names moved from ygg to urd.

#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd
{

/// What Hash returns: 64 bits on every platform, so hashes are the same everywhere.
using hash_t = std::uint64_t;

/// std::isnan, also in constant evaluation, where std::isnan is constexpr only from C++23: every value but NaN
/// lies within [-infinity, infinity].
template<std::floating_point T>
constexpr bool is_nan(T value) noexcept
{
    constexpr auto infinity = std::numeric_limits<T>::infinity();
    return !(value >= -infinity && value <= infinity);
}

template<typename T>
struct EqualTo;

template<typename T>
struct Hash;

template<typename T>
struct Less;

/// The members that identify a record, as member pointers:
/// `static constexpr auto identifying_members() noexcept { return urd::Members<&Data::a, &Data::b> {}; }`.
template<auto... Ms>
struct Members
{
};

namespace detail
{
template<typename L>
inline constexpr bool is_members_v = false;
template<auto... Ms>
inline constexpr bool is_members_v<Members<Ms...>> = true;
}  // namespace detail

/// @brief T lists its identifying members as a Members type.
template<typename T>
concept MemberIdentified = requires { T::identifying_members(); } && detail::is_members_v<decltype(T::identifying_members())>;

/// @brief T names its members as a Members type (cista_members), so cista can serialize it.
template<typename T>
concept Serializable = requires { T::cista_members(); } && detail::is_members_v<decltype(T::cista_members())>;

/// @brief T names what identifies it: its identifying members, or a function that returns what identifies it; Hash,
/// EqualTo and Less then follow from that.
template<typename T>
concept Identifiable = MemberIdentified<T> || requires(const T& x) { x.identifying_members(); };

namespace detail
{

/// What identifies `x`, as one value that Hash, EqualTo and Less read: its identifying members as a tuple of
/// references, or what its identifying_members() returns.
template<Identifiable T>
constexpr decltype(auto) identity(const T& x)
{
    if constexpr (MemberIdentified<T>)
        return []<auto... Ms>(const T& of, Members<Ms...> /*members*/) { return std::tie(of.*Ms...); }(x, T::identifying_members());
    else
        return x.identifying_members();
}
template<Identifiable T>
using identity_t = std::remove_cvref_t<decltype(identity(std::declval<const T&>()))>;

}  // namespace detail

/// Whether a range's elements, in the order it iterates them, are its value: Hash, EqualTo and Less then follow from
/// its elements'. True for the standard containers that keep an order of their own; a container type opts in by
/// specializing it (a hash table does not: its order is not its contents').
template<typename R>
inline constexpr bool value_range = false;
template<typename T, std::size_t N>
inline constexpr bool value_range<std::array<T, N>> = true;
template<typename T, typename Allocator>
inline constexpr bool value_range<std::vector<T, Allocator>> = true;
template<typename Key, typename Compare, typename Allocator>
inline constexpr bool value_range<std::set<Key, Compare, Allocator>> = true;
template<typename Key, typename T, typename Compare, typename Allocator>
inline constexpr bool value_range<std::map<Key, T, Compare, Allocator>> = true;
template<typename T, std::size_t Extent>
inline constexpr bool value_range<std::span<T, Extent>> = true;

template<typename T>
concept Enumeration = std::is_enum_v<T>;

template<typename T>
concept Hashable = requires(const T& value) {
    { Hash<std::remove_cvref_t<T>> {}(value) } -> std::same_as<hash_t>;
};

template<typename T>
concept EqualityComparableByEqualTo = requires(const T& lhs, const T& rhs) {
    { EqualTo<std::remove_cvref_t<T>> {}(lhs, rhs) } -> std::same_as<bool>;
};

template<typename T>
concept OrderedByLess = requires(const T& lhs, const T& rhs) {
    { Less<std::remove_cvref_t<T>> {}(lhs, rhs) } -> std::same_as<bool>;
};

}  // namespace urd
