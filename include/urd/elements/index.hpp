#pragma once

#include "urd/elements/concepts.hpp"
#include "urd/elements/requirements.hpp"

#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace urd
{

/// The number of a Store that hands out indices for views (a pool of records, a positional set): with an index, what
/// a view<T, C> is identified by, so that equal views are the same element whichever Store they were made in. Given
/// to the Store's constructor, by whoever builds it: Store(urd::layer { 3 }), or for a layered Store, its place along
/// the chain.
struct layer
{
    std::size_t number;
};

/// The index of a T in the Store that numbers Ts: a positional set or a pool mints one on first insert, so it is
/// dense over the elements (over the slots, for a pool that erases; over the nodes, for a tree pool's rows). It is a
/// dense key for tables keyed by Ts, and the first half of a view<T, C>. The default is none: no index at all, which a
/// dense lookup finds absent. An index of A is a different type from an index of B, and converts to neither.
template<typename T>
struct index
{
    using element_type = T;
    /// What it locates: an index is a handle (urd::handle).
    using located = T;
    using value_type = std::uint32_t;
    static constexpr value_type none_value = std::numeric_limits<value_type>::max();

    value_type value = none_value;

    constexpr index() = default;
    constexpr explicit index(value_type at) noexcept : value(at) {}

    [[nodiscard]] constexpr bool is_none() const noexcept { return value == none_value; }
    /// The record it locates, as the context holds it (what a view of it reads: urd::view).
    [[nodiscard]] decltype(auto) read(const auto& context) const { return context[*this]; }

    friend constexpr auto operator<=>(const index&, const index&) = default;
    static constexpr auto identifying_members() noexcept { return Members<&index::value> {}; }
    /// For records stored flat: cista serializes an index by its value.
    static constexpr auto cista_members() noexcept { return Members<&index::value> {}; }
};

/// A handle locates a T in a context, which resolves it (context[h] is the record, context.view(h) its view): it names
/// what it locates (`using located = T;`) and reads it, `h.read(context)` (context[h]), as a view's holding type does
/// (urd::view). urd::index<T> is one; a library's composite handle declares the same (a binding's relation and row).
template<typename H>
concept handle = requires { typename H::located; };

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

/// Whether T is an index<…> (index_of<K>::is_index asks whether a key is one).
template<typename T>
inline constexpr bool is_index_type = false;
template<typename T>
inline constexpr bool is_index_type<index<T>> = true;

/// A change to a row of a pool: the value at `position` becomes `value`. A pool of sequence rows takes a sorted
/// span of them, with unique positions, to insert a row that differs from another in few positions.
template<typename T>
struct Write
{
    std::size_t position = 0;
    T value {};
};

/// The Store or layer that resolves a T's indices by default: `template<> struct urd::context_of<Atom> { using
/// type = Repository; };` once per record type, so that view<Atom> needs no second argument.
template<typename T>
struct context_of;

/// A T read through a context, holding what reads its record (H): an index, or a record held outside the context
/// (urd/elements/view.hpp).
template<typename T, typename C = typename context_of<T>::type, typename H = index<T>>
class view;

}  // namespace urd
