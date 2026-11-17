#pragma once

#include "urd/config.hpp"
#include "urd/store/types.hpp"

#include <cassert>
#include <concepts>
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
    /// The elements of [first, last), copied: only of a copyable element (urd::detail::denied).
    template<std::input_iterator InputIt>
    void assign(InputIt first, InputIt last)
        requires std::copy_constructible<value_type>
    {
        representation().assign(first, last);
        assert(holds_one() && "urd::Store: a nonempty Store needs an element");
    }

    template<std::input_iterator InputIt,
             urd::fixed_string Reason = "assign(first, last): the element is move-only: build a Store of the elements moved in and move-assign it">
    void assign(InputIt first, InputIt last)
        requires denied<Reason, value_type>
    URD_DENY("assign(first, last): the element is move-only: build a Store of the elements moved in and move-assign it");

    /// `count` copies of `value`.
    void assign(std::size_t count, const value_type& value)
        requires std::copy_constructible<value_type>
    {
        representation().assign(count, value);
        assert(holds_one() && "urd::Store: a nonempty Store needs an element");
    }

    void assign(std::size_t count, const value_type& value)
        requires denied<"assign(n, const T&): the element is move-only: it is copied n times", value_type>
    URD_DENY("assign(n, const T&): the element is move-only: it is copied n times");

private:
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }

    /// A nonempty Store holds an element still.
    [[nodiscard]] bool holds_one() { return !store_types<S>::needs_an_element || !std::ranges::empty(representation()); }
};

}  // namespace urd::detail::facet
