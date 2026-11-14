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

#include <boost/container_hash/hash.hpp>
#include <cstddef>
#include <functional>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

// Hashes are deterministic for a fixed Boost version and platform word size: hash_combine uses
// Boost.ContainerHash's native-size mixing. The public result is 64-bit.

/// @brief `Hash` is our custom hasher, using explicit algorithms instead of implementation-defined std::hash.
///
/// There is deliberately no fallback to std::hash: its algorithms for floating-point and string types
/// differ between standard library implementations, which makes hash container iteration orders (and
/// everything order-sensitive built on top) platform-dependent. Unknown key types must either provide
/// identifying_members() or a urd::Hash specialization.
/// The primary template is deliberately left undefined: a type without a deterministic hash fails to
/// compile as an incomplete type. Provide identifying_members() or specialize urd::Hash for new types.
template<typename T>
struct Hash;

/// Mixes the hash of `value` into `seed`.
template<Hashable T>
inline void hash_combine(hash_t& seed, const T& value)
{
    auto combined = static_cast<size_t>(seed);
    boost::hash_combine(combined, Hash<std::remove_cvref_t<T>> {}(value));
    seed = combined;
}

/// The hash of a range's elements, in order, each mixed in (hash_combine): alike for every range of the same values,
/// whatever holds them (contiguous, segmented, decoded).
///
/// There is deliberately no faster path that hashes contiguous integers as one run of bytes (boost::hash_range over
/// the bytes, xxHash: 6 times faster at 4096 values). It would be a second algorithm, chosen by
/// the holder and not by the values: a key held in a span would take it, and the equal row a packed pool stores
/// (decoded value by value on access, so not contiguous) would not. The two would hash differently, and the pool's
/// table, which hashes a stored row as it is stored and a key as it is given, would lose the row when it rehashes.
/// One algorithm for every holder can be the byte one only by first copying each non-contiguous range side by side.
template<std::ranges::input_range Range>
    requires Hashable<std::ranges::range_value_t<Range>>
inline hash_t hash_range(Range&& range)
{
    hash_t seed = 0;
    for (const auto& value : std::forward<Range>(range))
        urd::hash_combine<std::ranges::range_value_t<Range>>(seed, value);  // converts proxy elements
    return seed;
}

template<std::integral T>
struct Hash<T>
{
    hash_t operator()(const T& el) const noexcept { return static_cast<hash_t>(el); }
};

template<Enumeration T>
struct Hash<T>
{
    hash_t operator()(const T& el) const
        requires Hashable<std::underlying_type_t<T>>
    { return Hash<std::underlying_type_t<T>> {}(static_cast<std::underlying_type_t<T>>(el)); }
};

template<std::floating_point T>
    requires std::same_as<T, float> || std::same_as<T, double>
struct Hash<T>
{
    /// The hash of every NaN: the 64-bit golden ratio constant.
    static constexpr hash_t nan_hash = 0x9e3779b97f4a7c15ULL;

    hash_t operator()(const T& el) const noexcept
    {
        // EqualTo treats all NaNs as equal; Boost already normalizes signed zero.
        return urd::is_nan(el) ? nan_hash : boost::hash<T> {}(el);
    }
};

/// Text hashes as its bytes, eight at a time (boost::hash_range). Unlike a range (hash_range), text has one holder:
/// every text type is contiguous and hashes through this one specialization (std::string, urd::flat_string), so no
/// other algorithm ever meets it. A range of chars that is not text hashes as hash_range gives it, not as text.
template<>
struct Hash<std::string_view>
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): the end of the text's el.size() bytes at el.data()
    hash_t operator()(std::string_view el) const noexcept { return boost::hash_range(el.data(), el.data() + el.size()); }
};

template<>
struct Hash<std::string>
{
    hash_t operator()(const std::string& el) const noexcept { return Hash<std::string_view> {}(el); }
};

/// A value range hashes as its elements, in order.
template<typename R>
    requires value_range<R>
struct Hash<R>
{
    hash_t operator()(const R& range) const
        requires Hashable<std::ranges::range_value_t<R>>
    { return urd::hash_range(range); }
};

template<typename T>
struct Hash<std::reference_wrapper<T>>
{
    hash_t operator()(const std::reference_wrapper<T>& ref) const
        requires Hashable<T>
    { return Hash<std::remove_cvref_t<T>> {}(ref.get()); }
};

template<typename... Ts>
struct Hash<std::tuple<Ts...>>
{
    hash_t operator()(const std::tuple<Ts...>& tuple) const
        requires(Hashable<Ts> && ...)
    {
        hash_t aggregated_hash = sizeof...(Ts);
        std::apply([&](const Ts&... args) { (urd::hash_combine(aggregated_hash, args), ...); }, tuple);
        return aggregated_hash;
    }
};

/// A pair (std's or cista's), as the tuple of its two members.
template<template<typename, typename> typename P, typename T1, typename T2>
    requires pair_value<P<T1, T2>>
struct Hash<P<T1, T2>>
{
    hash_t operator()(const P<T1, T2>& pair) const
        requires Hashable<T1> && Hashable<T2>
    { return Hash<std::tuple<const T1&, const T2&>> {}(std::tie(pair.first, pair.second)); }
};

/// A variant (std's or cista's) hashes the index it holds and the alternative.
template<template<typename...> typename V, typename... Ts>
    requires variant_value<V<Ts...>>
struct Hash<V<Ts...>>
{
    hash_t operator()(const V<Ts...>& variant) const
        requires(Hashable<Ts> && ...)
    {
        hash_t seed = variant.index();
        if (variant.index() < sizeof...(Ts))
            visit([&seed](const auto& alternative) { urd::hash_combine(seed, alternative); }, variant);
        return seed;
    }
};
/// An optional (std's or cista's) hashes whether it holds a value, and the value.
template<optional_value M>
struct Hash<M>
{
    hash_t operator()(const M& optional) const
        requires Hashable<optional_value_t<M>>
    {
        hash_t seed = optional.has_value() ? 1 : 0;
        if (optional.has_value())
            urd::hash_combine(seed, *optional);
        return seed;
    }
};
namespace detail
{

/// The hash of what identifies a type (detail::identity): of one member, that member's own hash, so that a type
/// wrapping one value (urd::index) hashes as the value does, without mixing it into a seed; of several, the tuple's.
template<typename M>
hash_t hash_identity(const std::tuple<M>& identity)
{ return Hash<std::remove_cvref_t<M>> {}(std::get<0>(identity)); }
template<typename... Ms>
hash_t hash_identity(const std::tuple<Ms...>& identity)
{ return Hash<std::tuple<Ms...>> {}(identity); }
/// The same hash, read from the element's members without a tuple of them.
template<typename T, auto... Ms>
hash_t hash_members(const T& element, Members<Ms...> /*members*/)
{
    if constexpr (sizeof...(Ms) == 1)
        return (Hash<std::remove_cvref_t<member_read_t<T, Ms>>> {}(std::invoke(Ms, element)), ...);
    else
    {
        hash_t seed = sizeof...(Ms);
        (urd::hash_combine(seed, std::invoke(Ms, element)), ...);
        return seed;
    }
}

}  // namespace detail

/// An identified type hashes as what identifies it (detail::identity); it also hashes what identifies it given alone
/// (detail::identity_t, or its parts by value), alike, so that a lookup may take it in place of the element.
template<Identifiable T>
struct Hash<T>
{
    using is_transparent = void;

    hash_t operator()(const T& element) const
        requires(detail::members_hashable<T>(T::identifying_members()))
    { return detail::hash_members(element, T::identifying_members()); }
    template<identity_of<T> U>
    hash_t operator()(const U& identity) const
        requires Hashable<detail::identity_t<T>>
    { return detail::hash_identity(identity); }
};

/// Hashes any hashable value by its type's Hash.
template<>
struct Hash<void>
{
    using is_transparent = void;

    template<Hashable T>
    hash_t operator()(const T& value) const
    { return Hash<std::remove_cvref_t<T>> {}(value); }
};

/// Mixes the hashes of `values` into `seed`, in order.
template<Hashable First, Hashable... Rest>
inline void hash_combine(hash_t& seed, const First& first, const Rest&... rest)
{
    urd::hash_combine(seed, first);
    (urd::hash_combine(seed, rest), ...);
}

/// The hashes of `values` mixed in order into a zero seed.
template<Hashable... Ts>
inline hash_t hash_combine(const Ts&... values)
{
    hash_t seed = 0;
    (urd::hash_combine(seed, values), ...);
    return seed;
}

}  // namespace urd
