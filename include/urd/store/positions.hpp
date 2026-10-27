#pragma once

#include "urd/store/types.hpp"

#include <cassert>
#include <cstddef>
#include <iterator>
#include <stdexcept>

namespace urd::detail::facet
{

/// A sequence: its elements by position, changeable in place where the Store's elements are.
template<typename Derived, typename R, typename S>
class positions
{
    using types = store_types<R, S>;
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

private:
    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
    [[nodiscard]] Derived& self() { return static_cast<Derived&>(*this); }
};

}  // namespace urd::detail::facet
