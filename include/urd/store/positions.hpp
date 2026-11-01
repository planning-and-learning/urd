#pragma once

#include "urd/elements/index.hpp"
#include "urd/store/types.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>

namespace urd::detail::facet
{

/// Positional: its elements by position, changeable in place where the Store's elements are.
template<typename Derived, typename R, typename S>
class positions
{
    using types = store_types<S>;
    using reference_type = typename iterator_types<R, S>::reference;
    using const_reference_type = typename iterator_types<R, S>::const_reference;
    static constexpr bool writable = types::writable;

public:
    [[nodiscard]] const_reference_type operator[](std::size_t pos) const
    {
        assert(pos < self().size());
        return self().begin()[static_cast<std::ptrdiff_t>(pos)];
    }
    [[nodiscard]] reference_type operator[](std::size_t pos)
        requires writable
    {
        assert(pos < self().size());
        return self().begin()[static_cast<std::ptrdiff_t>(pos)];
    }
    [[nodiscard]] const_reference_type at(std::size_t pos) const
    {
        if (pos >= self().size())
            throw std::out_of_range("urd::Store::at: position out of range");
        return (*this)[pos];
    }
    [[nodiscard]] reference_type at(std::size_t pos)
        requires writable
    {
        if (pos >= self().size())
            throw std::out_of_range("urd::Store::at: position out of range");
        return (*this)[pos];
    }
    /// The position of the element at `it`: in a set, its index; size() for end(), as find gives for an absent one.
    [[nodiscard]] std::size_t position(typename iterator_types<R, S>::const_iterator it) const { return static_cast<std::size_t>(it - self().begin()); }

private:
    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
    [[nodiscard]] Derived& self() { return static_cast<Derived&>(*this); }
};

/// A set's positions: where it asks for them, or where it numbers its keys as they come and never frees one
/// (insertion_ordered without reclaims, an interner), so that a key's position is its number.
template<typename Derived, typename R, typename S>
class insertion_positions : public positions<Derived, R, S>
{
    using positions_base = positions<Derived, R, S>;
    using types = store_types<S>;
    using const_reference_type = typename iterator_types<R, S>::const_reference;

public:
    /// A set's elements by index (urd::index<T>): what a positional set mints, and what a view<T, Store> holds.
    using index_type = urd::index<typename types::value_type>;

    using positions_base::operator[];
    using positions_base::at;

    [[nodiscard]] const_reference_type operator[](index_type at) const { return (*this)[static_cast<std::size_t>(at.value)]; }
    [[nodiscard]] const_reference_type at(index_type at) const
    {
        if (at.is_none())
            throw std::out_of_range("urd::Store::at: none is no index");
        return this->at(static_cast<std::size_t>(at.value));
    }
    /// The index of the element at `it`.
    [[nodiscard]] index_type index(typename iterator_types<R, S>::const_iterator it) const
    { return index_type(static_cast<std::uint32_t>(this->position(it))); }
    /// A view of the element at `at`, through this Store (urd/elements/view.hpp must be included where views are made).
    [[nodiscard]] urd::view<typename types::value_type, Derived> view(index_type at) const
    {
        static_assert(requires { sizeof(urd::view<typename types::value_type, Derived>); }, "urd::Store::view: include <urd/elements/view.hpp>");
        return urd::view<typename types::value_type, Derived>(at, static_cast<const Derived&>(*this));
    }
    /// Its number, as it was built with (urd::layer): with an index, what its views are identified by.
    [[nodiscard]] std::size_t layer() const { return store_access::of<Derived>(*this).layer(); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: its kind keeps its elements by position (a sequence, a multiset), or it asks for
/// positions.
template<>
struct facet_condition<facet::positions> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, positional>; }
};
template<>
struct facet_operations<facet::positions>
{
    using offered = operation_list<operation::access>;
};
/// When a set has positions: it asks for them, or it is insertion-ordered and never frees a key (kept in one
/// array: an interner, a sparse set), so its keys have positions whether the request names them or not.
template<>
struct facet_condition<facet::insertion_positions> : facet_defaults
{
    /// It hands out indices for views, so the Store's container is numbered (urd::layer).
    static constexpr bool hands_out_indices = true;
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, positional> || (has<S, insertion_ordered> && !has<S, reclaims>); }
};
template<>
struct facet_operations<facet::insertion_positions>
{
    using offered = operation_list<operation::access>;
};

}  // namespace urd::detail
