#pragma once

#include "urd/store/types.hpp"

#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace urd::detail::facet
{

/// Growable: inserting, by value as std::set or std::multiset does, or at the end as std::vector does. A sequence
/// that may also erase inserts before a position too; it moves the later elements.
template<typename Derived, typename R, typename S>
class growth
{
    using types = store_types<R, S>;
    using value_type = typename types::value_type;
    using iterator_type = typename iterator_types<R, S>::iterator;
    using const_iterator_type = typename iterator_types<R, S>::const_iterator;
    static constexpr bool keyed = types::keyed;

public:
    using insert_result = std::conditional_t<has<S, unique>, std::pair<iterator_type, bool>, iterator_type>;

    insert_result insert(const value_type& element)
        requires keyed
    { return representation().insert(element); }
    insert_result insert(value_type&& element)
        requires keyed
    { return representation().insert(std::move(element)); }
    /// With a hint where the element goes.
    iterator_type insert(const_iterator_type hint, const value_type& element)
        requires keyed
    { return representation().insert(hint, element); }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
        requires keyed
    { representation().insert(first, last); }
    void insert(std::initializer_list<value_type> elements)
        requires keyed
    { insert(elements.begin(), elements.end()); }
    template<typename... Args>
    insert_result emplace(Args&&... args)
        requires keyed
    { return representation().emplace(std::forward<Args>(args)...); }

    void push_back(const value_type& element)
        requires(!keyed)
    { representation().push_back(element); }
    void push_back(value_type&& element)
        requires(!keyed)
    { representation().push_back(std::move(element)); }
    /// The new element; changeable in place only where the Store's elements are.
    template<typename... Args>
    typename iterator_types<R, S>::reference emplace_back(Args&&... args)
        requires(!keyed)
    { return representation().emplace_back(std::forward<Args>(args)...); }
    iterator_type insert(const_iterator_type pos, const value_type& element)
        requires(!keyed && has<S, shrinkable>)
    { return representation().insert(pos, element); }
    template<typename... Args>
    iterator_type emplace(const_iterator_type pos, Args&&... args)
        requires(!keyed && has<S, shrinkable>)
    { return representation().emplace(pos, std::forward<Args>(args)...); }

private:
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet
