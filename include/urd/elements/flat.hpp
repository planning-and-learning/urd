#pragma once

// Records stored flat: the cista containers a record is written with, under urd's names, their hash and equality,
// and how a view resolves them. Include it where flat records are defined; the pools that store them are in
// urd/repr/records/records.hpp and urd/repr/records/arena.hpp, included where such a Store is held.

#include "urd/elements/concepts.hpp"
#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/view.hpp"
#include "urd/memory.hpp"

#include <boost/mp11/algorithm.hpp>
#include <cista/containers/optional.h>
#include <cista/containers/pair.h>
#include <cista/containers/string.h>
#include <cista/containers/variant.h>
#include <cista/containers/vector.h>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

/// The containers of a record stored flat: cista's, which serialize as a whole.
template<typename T>
using flat_vector = ::cista::offset::vector<T>;
using flat_string = ::cista::offset::string;
template<typename... Ts>
using flat_variant = ::cista::offset::variant<Ts...>;
template<typename T>
using flat_optional = ::cista::optional<T>;
template<typename T1, typename T2>
using flat_pair = ::cista::pair<T1, T2>;

/// Whether T is one of the flat containers above.
template<typename T>
inline constexpr bool is_flat_vector = false;
template<typename T>
inline constexpr bool is_flat_vector<flat_vector<T>> = true;
template<typename T>
inline constexpr bool is_flat_variant = false;
template<typename... Ts>
inline constexpr bool is_flat_variant<flat_variant<Ts...>> = true;
template<typename T>
inline constexpr bool is_flat_optional = false;
template<typename T>
inline constexpr bool is_flat_optional<flat_optional<T>> = true;
template<typename T>
inline constexpr bool is_flat_pair = false;
template<typename T1, typename T2>
inline constexpr bool is_flat_pair<flat_pair<T1, T2>> = true;
/// The flat_variant of the indices of a type list's tags.
template<typename L>
using index_variant = boost::mp11::mp_apply<flat_variant, boost::mp11::mp_transform<index, L>>;

/// Text hashes as its bytes (Hash<std::string_view>), so equal text hashes alike in every representation; it compares
/// by cista's ==.
template<>
struct Hash<flat_string>
{
    hash_t operator()(const flat_string& text) const noexcept { return Hash<std::string_view> {}(text.view()); }
};

/// Text owns its characters where they do not fit inside it (cista allocates exactly its length).
template<>
struct memory_of<flat_string>
{
    static std::size_t bytes(const flat_string& text) { return text.is_short() ? 0 : text.size(); }
};

/// A flat vector owns the elements it allocated (none where it reads a buffer it was loaded from), and what they own.
template<typename T>
struct memory_of<flat_vector<T>>
{
    static std::size_t bytes(const flat_vector<T>& held)
    { return (held.self_allocated_ ? held.allocated_size_ * sizeof(T) : std::size_t { 0 }) + detail::held_bytes(held); }
};

/// A record that names its members (as flat storage needs) holds what they hold.
template<Serializable T>
struct memory_of<T>
{
    static std::size_t bytes(const T& record)
    { return []<auto... Ms>(Members<Ms...> /*members*/, const T& of) { return (std::size_t { 0 } + ... + memory_usage(of.*Ms)); }(T::cista_members(), record); }
};

/// A flat vector is a value range: hashed, compared and ordered as its elements.
template<typename T, template<typename> typename Ptr, bool IndexPointers, typename SizeType, class Allocator>
inline constexpr bool value_range<::cista::basic_vector<T, Ptr, IndexPointers, SizeType, Allocator>> = true;

namespace detail
{
template<typename T, template<typename...> typename Of>
inline constexpr bool specializes = false;
template<template<typename...> typename Of, typename... Ts>
inline constexpr bool specializes<Of<Ts...>, Of> = true;
}  // namespace detail

/// The stored form of a readout, the inverse of urd::resolve: what a record holds for what a view reads. A variant
/// (std's or cista's) gives the flat_variant of its alternatives' stored forms, a std::optional the flat_optional, a
/// pair the flat_pair; a view of an element held by index gives its index (index()), a view of a record held elsewhere
/// a copy of its record (data()); a range (not text) gives the flat_vector of its elements'; anything else is its own
/// stored form.
template<typename R>
[[nodiscard]] auto to_stored(const R& readout)
{
    if constexpr (variant_value<R>)
        return []<template<typename...> typename V, typename... Vs>(const V<Vs...>& alternatives)
        {
            using stored = flat_variant<decltype(urd::to_stored(std::declval<const Vs&>()))...>;
            return visit([](const auto& alternative) { return stored(urd::to_stored(alternative)); }, alternatives);
        }(readout);
    else if constexpr (detail::specializes<R, std::optional>)
    {
        using stored = flat_optional<decltype(urd::to_stored(*readout))>;
        return readout.has_value() ? stored(urd::to_stored(*readout)) : stored();
    }
    else if constexpr (pair_value<R>)
        return flat_pair<decltype(urd::to_stored(readout.first)), decltype(urd::to_stored(readout.second))> { urd::to_stored(readout.first),
                                                                                                                urd::to_stored(readout.second) };
    else if constexpr (requires { readout.index(); })
        return readout.index();
    else if constexpr (requires { requires std::is_class_v<std::remove_cvref_t<decltype(readout.data())>>; })
        return readout.data();
    else if constexpr (std::ranges::input_range<const R> && !std::convertible_to<const R&, std::string_view>)
    {
        auto stored = flat_vector<decltype(urd::to_stored(*std::ranges::begin(readout)))>();
        if constexpr (std::ranges::sized_range<const R>)
            stored.reserve(static_cast<typename decltype(stored)::size_type>(std::ranges::size(readout)));
        for (const auto& element : readout)
            stored.push_back(urd::to_stored(element));
        return stored;
    }
    else
        return readout;
}

/// The type to_stored gives for a readout of type R.
template<typename R>
using stored_t = decltype(urd::to_stored(std::declval<const R&>()));

}  // namespace urd
