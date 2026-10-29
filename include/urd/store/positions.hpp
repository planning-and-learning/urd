#pragma once

#include "urd/index.hpp"
#include "urd/store/types.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>

namespace urd::detail::facet
{

/// Positional (a sequence, or a set with positions, such as an interner): its elements by position, changeable in
/// place where the Store's elements are.
template<typename Derived, typename R, typename S>
class positions
{
    using types = store_types<R, S>;
    using reference_type = typename iterator_types<R, S>::reference;
    using const_reference_type = typename iterator_types<R, S>::const_reference;
    static constexpr bool writable = types::writable;

public:
    /// A set's elements by index (urd::index<T>): what a positional set mints, and what a view<T, Store> holds.
    using index_type = urd::index<typename types::value_type>;

    [[nodiscard]] const_reference_type operator[](index_type at) const
        requires S::is_set
    {
        return (*this)[static_cast<std::size_t>(at.value)];
    }
    [[nodiscard]] const_reference_type at(index_type at) const
        requires S::is_set
    {
        if (at.is_none())
            throw std::out_of_range("urd::Store::at: none is no index");
        return this->at(static_cast<std::size_t>(at.value));
    }
    /// The index of the element at `it`.
    [[nodiscard]] index_type index(typename iterator_types<R, S>::const_iterator it) const
        requires S::is_set
    {
        return index_type(static_cast<std::uint32_t>(position(it)));
    }
    /// A view of the element at `at`, through this Store (urd/view.hpp must be included where views are made).
    [[nodiscard]] auto view(index_type at) const
        requires S::is_set
    {
        static_assert(requires { sizeof(urd::view<typename types::value_type, Derived>); }, "urd::Store::view: include <urd/view.hpp>");
        return urd::view<typename types::value_type, Derived>(at, self());
    }
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
    /// The first element; a nonempty Store always has one, any other needs one.
    [[nodiscard]] const_reference_type front() const
    {
        assert(!self().empty());
        return *self().begin();
    }
    [[nodiscard]] reference_type front()
        requires writable
    {
        assert(!self().empty());
        return *self().begin();
    }
    [[nodiscard]] const_reference_type back() const
    {
        assert(!self().empty());
        return *std::prev(self().end());
    }
    [[nodiscard]] reference_type back()
        requires writable
    {
        assert(!self().empty());
        return *std::prev(self().end());
    }

    /// The position of the element at `it`: in a set, its index; size() for end(), as find gives for an absent one.
    [[nodiscard]] std::size_t position(typename iterator_types<R, S>::const_iterator it) const { return static_cast<std::size_t>(it - self().begin()); }

private:
    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
    [[nodiscard]] Derived& self() { return static_cast<Derived&>(*this); }
};

}  // namespace urd::detail::facet
