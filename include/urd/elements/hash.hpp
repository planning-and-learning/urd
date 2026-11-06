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
#include <boost/hash2/xxhash.hpp>
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

// Hashes are deterministic for a fixed Boost version and platform word size. Byte-hashed ranges
// additionally depend on their element representation. The public result is 64-bit; hash_combine
// uses Boost.ContainerHash's native-size mixing.

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

/// Deterministic hashing primitives with fixed algorithms and constants.
namespace hashing
{

/// xxHash64 with a fixed zero seed over a byte range.
inline hash_t hash_bytes(const void* data, size_t size) noexcept
{
    auto hash = boost::hash2::xxhash_64 {};
    hash.update(data, size);
    return hash.result();
}

/// Byte equality must agree with value equality; custom Hash specializations retain value hashing.
template<typename T>
concept ByteHashable = (std::integral<T> || Enumeration<T>)
                       && !std::same_as<T, bool> && std::has_unique_object_representations_v<T> && requires { requires Hash<T>::is_identity; };

}  // namespace hashing

/// Mixes the hash of `value` into `seed`.
template<Hashable T>
inline void hash_combine(hash_t& seed, const T& value)
{
    auto combined = static_cast<size_t>(seed);
    boost::hash_combine(combined, Hash<std::remove_cvref_t<T>> {}(value));
    seed = combined;
}

/// The hash of a range's elements, in order. Elements hashed as their bytes (integers, enums) are hashed as the bytes
/// of their values, contiguous or not, so that decoded and segmented ranges hash as contiguous ones of the same
/// element type; any other element is mixed in one by one.
namespace detail
{
/// Whether hash_range hashes a range as one block of bytes: contiguous, sized, of byte-hashed elements.
template<typename Range>
inline constexpr bool hashed_as_bytes = hashing::ByteHashable<std::ranges::range_value_t<Range>> && std::ranges::sized_range<Range>
                                        && std::ranges::contiguous_range<Range>
                                        && !std::is_volatile_v<std::remove_reference_t<std::ranges::range_reference_t<Range>>>;
/// Whether hash_range of such a range throws nothing: taking its data and size throws nothing.
template<typename Range>
consteval bool hashes_without_throwing()
{
    if constexpr (hashed_as_bytes<Range>)
        return noexcept(std::ranges::data(std::declval<Range&>())) && noexcept(std::ranges::size(std::declval<Range&>()));
    else
        return false;
}
}  // namespace detail

/// Hashing a block of bytes throws only where taking the range's data or size does.
template<std::ranges::input_range Range>
    requires Hashable<std::ranges::range_value_t<Range>>
inline hash_t hash_range(Range&& range) noexcept(detail::hashes_without_throwing<Range>())
{
    using Value = std::ranges::range_value_t<Range>;
    if constexpr (detail::hashed_as_bytes<Range>)
    {
        // A span of const elements also takes rvalue ranges; std::views::all would need a copy of
        // a const view, which move-only views do not have.
        const auto view = std::span<const Value>(std::forward<Range>(range));
        return hashing::hash_bytes(view.data(), view.size_bytes());
    }
    else if constexpr (hashing::ByteHashable<Value>)
    {
        auto hash = boost::hash2::xxhash_64 {};
        for (Value value : std::forward<Range>(range))
            hash.update(&value, sizeof(value));
        return hash.result();
    }
    else
    {
        hash_t seed = 0;
        if constexpr (std::ranges::sized_range<Range>)
            seed = std::ranges::size(range);
        for (const auto& value : std::forward<Range>(range))
            urd::hash_combine<Value>(seed, value);  // converts proxy elements
        return seed;
    }
}

template<std::integral T>
struct Hash<T>
{
    // Custom specializations omit this marker and retain element-wise range hashing.
    static constexpr bool is_identity = true;

    hash_t operator()(const T& el) const noexcept { return static_cast<hash_t>(el); }
};

template<Enumeration T>
struct Hash<T>
{
    static constexpr bool is_identity = true;

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

/// Text hashes as its bytes: what hash_range gives a contiguous range of chars, so equal text hashes alike in every
/// representation (urd::flat_string too).
template<>
struct Hash<std::string_view>
{
    hash_t operator()(std::string_view el) const noexcept { return hashing::hash_bytes(el.data(), el.size()); }
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
        std::apply([&aggregated_hash](const Ts&... args) { (urd::hash_combine(aggregated_hash, args), ...); }, tuple);
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
/// An identified type hashes as what identifies it (detail::identity); it also hashes what identifies it given alone
/// (detail::identity_t, or its parts by value), alike, so that a lookup may take it in place of the element.
template<Identifiable T>
struct Hash<T>
{
    using is_transparent = void;

    hash_t operator()(const T& element) const
        requires Hashable<detail::identity_t<T>>
    { return Hash<detail::identity_t<T>> {}(detail::identity(element)); }
    template<identity_of<T> U>
    hash_t operator()(const U& identity) const
        requires Hashable<detail::identity_t<T>>
    { return Hash<detail::identity_t<T>> {}(identity); }
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
