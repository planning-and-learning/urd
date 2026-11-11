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

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
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

/// With T void (EqualTo<>, Hash<>, Less<>): transparent, by the type of what they are given.
template<typename T = void>
struct EqualTo;

template<typename T = void>
struct Hash;

template<typename T = void>
struct Less;

/// The members that identify a record: data members, const getters, or captureless lambdas over the record for a part
/// that is computed or held elsewhere (each read by std::invoke):
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

/// @brief T names its members as a Members type (cista_members), so cista can serialize it.
template<typename T>
concept Serializable = requires { T::cista_members(); } && detail::is_members_v<decltype(T::cista_members())>;
/// @brief Empties every member a record names for cista (Serializable), keeping the buffers they own: each by its
/// clear(), or else set to its default. A record reused in place (a builder) is reset with it.
template<Serializable T>
void clear_members(T& record)
{
    const auto clear = [](auto& member)
    {
        if constexpr (requires { member.clear(); })
            member.clear();
        else
            member = std::remove_cvref_t<decltype(member)> {};
    };
    [&]<auto... Ms>(Members<Ms...> /*members*/) { (clear(record.*Ms), ...); }(T::cista_members());
}

/// @brief T names what identifies it, as a Members list (its identifying_members()); Hash, EqualTo and Less then
/// follow from that.
template<typename T>
concept Identifiable = requires { T::identifying_members(); } && detail::is_members_v<decltype(T::identifying_members())>;

namespace detail
{

/// What identifies `x`, as one value that Hash, EqualTo and Less read: its identifying members, in order, as a tuple
/// (a data member by reference, a getter's result as the getter gives it).
template<Identifiable T>
constexpr auto identity(const T& x)
{
    return []<auto... Ms>(const T& of, Members<Ms...> /*members*/)
    { return std::tuple<decltype(std::invoke(Ms, of))...>(std::invoke(Ms, of)...); }(x, T::identifying_members());
}
template<Identifiable T>
using identity_t = decltype(identity(std::declval<const T&>()));

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

/// A hasher H of T, an equality E of Lhs and Rhs, an order C of Lhs and Rhs: what a container takes as its predicates.
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

/// U is what identifies T, given alone (its identity_t: data members by reference, as std::tie gives them, getters' results
/// as they give them), so a transparent lookup takes a U for a T.
template<typename U, typename T>
concept identity_of = Identifiable<T> && std::same_as<std::remove_cvref_t<U>, detail::identity_t<T>>;

/// Whether M is an optional value: std::optional, or cista's.
template<typename M>
concept optional_value = requires(const M& member) {
    { member.has_value() } -> std::convertible_to<bool>;
    *member;
};

/// Whether M is a pair: std::pair, or cista's.
template<typename M>
concept pair_value = requires(const M& member) {
    typename M::first_type;
    typename M::second_type;
    member.first;
    member.second;
};

/// Whether M is a variant: std::variant, or cista's, each visited by its own visit (found by ADL).
template<typename M>
concept variant_value = requires(const M& member) {
    { member.index() } -> std::convertible_to<std::size_t>;
    visit([](const auto&) {}, member);
};

/// The value type of an optional value.
template<optional_value M>
using optional_value_t = std::remove_cvref_t<decltype(*std::declval<const M&>())>;

}  // namespace urd
