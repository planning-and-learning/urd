#pragma once

#include "urd/store/types.hpp"

#include <cassert>
#include <cstddef>
#include <iterator>
#include <ranges>

namespace urd::detail::facet
{

/// Replacing a sequence's contents in place, its storage kept, as std::vector::assign: the one way a frozen sequence
/// changes. A Store of another kind is built anew and assigned.
template<typename Derived, typename R, typename S>
class assigning
{
    using value_type = typename store_types<S>::value_type;

public:
    /// The elements of [first, last).
    template<std::input_iterator InputIt>
    void assign(InputIt first, InputIt last)
    {
        representation().assign(first, last);
        assert(holds_one() && "urd::Store: a nonempty Store needs an element");
    }
    /// `count` copies of `value`.
    void assign(std::size_t count, const value_type& value)
    {
        representation().assign(count, value);
        assert(holds_one() && "urd::Store: a nonempty Store needs an element");
    }

private:
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
    /// A nonempty Store holds an element still.
    [[nodiscard]] bool holds_one() { return !store_types<S>::needs_an_element || !std::ranges::empty(representation()); }
};

}  // namespace urd::detail::facet
