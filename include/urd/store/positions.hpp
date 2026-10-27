#pragma once

#include "urd/store/types.hpp"

#include <cassert>
#include <iterator>
#include <stdexcept>

namespace urd::detail::facet
{

/// A sequence: its elements by position, changeable in place where the Store's elements are.
template<typename Derived, typename R, typename S>
class positions
{
    using types = store_types<R, S>;
    using size_type = typename types::size_type;
    using difference_type = typename types::difference_type;
    using reference = typename types::reference;
    using const_reference = typename types::const_reference;
    static constexpr bool writable = types::writable;

public:
    [[nodiscard]] const_reference operator[](size_type pos) const
    {
        assert(pos < self().size());
        return self().begin()[static_cast<difference_type>(pos)];
    }
    [[nodiscard]] reference operator[](size_type pos)
        requires writable
    {
        assert(pos < self().size());
        return self().begin()[static_cast<difference_type>(pos)];
    }
    [[nodiscard]] const_reference at(size_type pos) const
    {
        if (pos >= self().size())
            throw std::out_of_range("urd::Store::at: position out of range");
        return (*this)[pos];
    }
    [[nodiscard]] reference at(size_type pos)
        requires writable
    {
        if (pos >= self().size())
            throw std::out_of_range("urd::Store::at: position out of range");
        return (*this)[pos];
    }
    /// The first element; a nonempty Store always has one, any other needs one.
    [[nodiscard]] const_reference front() const
    {
        assert(!self().empty());
        return *self().begin();
    }
    [[nodiscard]] reference front()
        requires writable
    {
        assert(!self().empty());
        return *self().begin();
    }
    [[nodiscard]] const_reference back() const
    {
        assert(!self().empty());
        return *std::prev(self().end());
    }
    [[nodiscard]] reference back()
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
