#pragma once

#include "urd/cost.hpp"
#include "urd/elements/equal_to.hpp"
#include "urd/store/types.hpp"

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace urd::detail::facet
{

/// Keeping elements where they are put, as std::vector does: appending, and where the Store shrinks, inserting before
/// a position (moving the later elements), removing the last, resizing, and erasing by value. What a sequence lists.
template<typename Derived, typename R, typename S>
class appending
{
    using types = store_types<S>;
    using value_type = typename types::value_type;
    using iterator_type = typename iterator_types<R, S>::iterator;
    using const_iterator_type = typename iterator_types<R, S>::const_iterator;

public:
    void push_back(const value_type& element) { representation().push_back(element); }
    void push_back(value_type&& element) { representation().push_back(std::move(element)); }
    /// The new element; changeable in place only where the Store's elements are.
    template<typename... Args>
    typename iterator_types<R, S>::reference emplace_back(Args&&... args)
    { return representation().emplace_back(std::forward<Args>(args)...); }
    iterator_type insert(const_iterator_type pos, const value_type& element)
        requires changes_in_the_middle<S>
    { return representation().insert(pos, element); }
    /// Copies of [first, last) before `pos`, as std::vector::insert.
    template<std::input_iterator InputIt>
    iterator_type insert(const_iterator_type pos, InputIt first, InputIt last)
        requires changes_in_the_middle<S>
    { return representation().insert(pos, first, last); }
    template<typename... Args>
    iterator_type emplace(const_iterator_type pos, Args&&... args)
        requires changes_in_the_middle<S>
    { return representation().emplace(pos, std::forward<Args>(args)...); }
    void pop_back()
        requires shrinks<S>
    { representation().pop_back(); }
    /// Grows to `count` elements, value-initialized or copies of `value`, or removes those after the first `count`, as
    /// std::vector::resize.
    void resize(std::size_t count)
        requires shrinks<S>
    { representation().resize(count); }
    void resize(std::size_t count, const value_type& value)
        requires shrinks<S>
    { representation().resize(count, value); }
    /// Every element equal to `element` (urd::EqualTo, as the Store's ==); how many. As std::erase.
    friend std::size_t erase(Derived& store, const value_type& element)
        requires changes_in_the_middle<S>
    {
        return erase_if(store, [&](const value_type& candidate) { return EqualTo<value_type> {}(candidate, element); });
    }

private:
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::appending> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return grows<S>; }
};
template<>
struct facet_operations<facet::appending>
{
    using offered = operation_list<operation::insert>;
};

}  // namespace urd::detail
