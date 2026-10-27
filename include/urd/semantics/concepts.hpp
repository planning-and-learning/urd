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

#ifndef URD_SEMANTICS_CONCEPTS_HPP_
#define URD_SEMANTICS_CONCEPTS_HPP_

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

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

/// @brief T has a function that returns the members that identify it.
template<typename T>
concept FunctionIdentified = !MemberIdentified<T> && requires(const T a) {
    { a.identifying_members() };
};

/// @brief T names what identifies it, in either way; Hash, EqualTo and Less then follow from that.
template<typename T>
concept Identifiable = FunctionIdentified<T> || MemberIdentified<T>;

/// @brief The type returned by identifying_members().
template<FunctionIdentified T>
using IdentifyingMembers = decltype(std::declval<const T&>().identifying_members());

template<typename T>
concept Enumeration = std::is_enum_v<T>;

template<typename T, typename U>
concept SameAsIgnoringCvref = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

template<typename H, typename T>
concept HashFor = requires(const H& hash, const T& value) {
    { hash(value) } -> std::same_as<hash_t>;
};

template<typename E, typename Lhs, typename Rhs = Lhs>
concept EqualToFor = requires(const E& equal_to, const Lhs& lhs, const Rhs& rhs) {
    { equal_to(lhs, rhs) } -> std::same_as<bool>;
};

template<typename C, typename Lhs, typename Rhs = Lhs>
concept LessFor = requires(const C& less, const Lhs& lhs, const Rhs& rhs) {
    { less(lhs, rhs) } -> std::same_as<bool>;
};

template<typename T>
concept Hashable = HashFor<Hash<std::remove_cvref_t<T>>, T>;

template<typename T>
concept EqualityComparableByEqualTo = EqualToFor<EqualTo<std::remove_cvref_t<T>>, T>;

template<typename T>
concept OrderedByLess = LessFor<Less<std::remove_cvref_t<T>>, T>;

}  // namespace urd

#endif
