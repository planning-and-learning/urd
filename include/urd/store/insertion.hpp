#pragma once

#include "urd/cost.hpp"
#include "urd/elements/prehashed.hpp"
#include "urd/store/types.hpp"

#include <concepts>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace urd::detail::facet
{

/// Placing elements by their value or key, as std::set, std::multiset and std::map do: inserting. What a set, a map and
/// a multiset list (removal erases them by value or key).
template<typename Derived, typename R, typename S>
class insertion
{
    using types = store_types<S>;
    using value_type = typename types::value_type;
    using iterator_type = typename iterator_types<R, S>::iterator;
    using const_iterator_type = typename iterator_types<R, S>::const_iterator;

public:
    using insert_result = std::conditional_t<has<S, unique>, std::pair<iterator_type, bool>, iterator_type>;

    insert_result insert(const value_type& element) { return representation().insert(element); }
    insert_result insert(value_type&& element) { return representation().insert(std::move(element)); }
    /// A copy of the key where it is new, by a key with its hash (urd/elements/prehashed.hpp), which is not hashed
    /// again: another form of a key, forwarded as it is (a representation that inserts by none does not compile).
    insert_result insert(const prehashed<value_type>& key) { return representation().insert(key); }
    /// With a hint where the element goes.
    iterator_type insert(const_iterator_type hint, const value_type& element) { return representation().insert(hint, element); }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    { representation().insert(first, last); }
    void insert(std::initializer_list<value_type> elements) { insert(elements.begin(), elements.end()); }
    template<typename... Args>
    insert_result emplace(Args&&... args)
    { return representation().emplace(std::forward<Args>(args)...); }

private:
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::insertion> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return grows<S>; }
};
template<>
struct facet_operations<facet::insertion>
{
    using offered = operation_list<operation::insert>;
};

}  // namespace urd::detail
