#pragma once

#include "urd/elements.hpp"
#include "urd/semantics/concepts.hpp"

#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace urd
{

/// The index of a T in the Store that numbers Ts: a positional set or a pool mints one on first insert, so it is
/// dense over the elements (over the slots, for a pool that erases; over the nodes, for a tree pool's rows). It is a
/// dense key for tables keyed by Ts, and the first half of a view<T, C>. The default is none: no index at all, which a
/// dense lookup finds absent. An index of A is a different type from an index of B, and converts to neither.
template<typename T>
struct index
{
    using element_type = T;
    using value_type = std::uint32_t;
    static constexpr value_type none_value = std::numeric_limits<value_type>::max();

    value_type value = none_value;

    constexpr index() = default;
    constexpr explicit index(value_type at) noexcept : value(at) {}

    [[nodiscard]] static constexpr index none() noexcept { return index(); }
    [[nodiscard]] constexpr bool is_none() const noexcept { return value == none_value; }
    constexpr explicit operator value_type() const noexcept { return value; }

    friend constexpr auto operator<=>(const index&, const index&) = default;
    static constexpr auto identifying_members() noexcept { return Members<&index::value> {}; }
};

/// An index is a dense key; none is no index, so a lookup of it finds nothing.
template<typename T>
struct index_of<index<T>>
{
    [[nodiscard]] static constexpr bool is_index(index<T> at) noexcept { return !at.is_none(); }
    [[nodiscard]] static constexpr std::size_t to_index(index<T> at) noexcept
    {
        assert(is_index(at) && "urd::index_of: none is no index");
        return at.value;
    }
    [[nodiscard]] static constexpr index<T> from_index(std::size_t at) noexcept { return index<T>(static_cast<std::uint32_t>(at)); }
};

template<typename T>
inline constexpr bool is_index = false;
template<typename T>
inline constexpr bool is_index<index<T>> = true;

/// The Store, Family or layer that resolves a T's indices by default: `template<> struct urd::context_of<Atom> { using
/// type = Repository; };` once per record type, so that view<Atom> needs no second argument.
template<typename T>
struct context_of;

/// A reference to an element held by index: the index, and the context that resolves it (urd/view.hpp).
template<typename T, typename C = typename context_of<T>::type>
class view;

}  // namespace urd
