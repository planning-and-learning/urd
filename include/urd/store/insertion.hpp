#pragma once

#include "urd/config.hpp"
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

    // An element is moved in by the && forms, only where it is new: one already held leaves the argument as it was. The
    // const& and range forms copy, and are deleted for a move-only element (urd::detail::denied).
    insert_result insert(const value_type& element)
        requires std::copy_constructible<value_type>
    { return representation().insert(element); }

    insert_result insert(const value_type& element)
        requires denied<"insert(const T&): the element is move-only: move it in, insert(std::move(x))", value_type>
    URD_DENY("insert(const T&): the element is move-only: move it in, insert(std::move(x))");

    insert_result insert(value_type&& element)
        requires std::move_constructible<value_type>
    { return representation().insert(std::move(element)); }

    /// A map's element holds its key const: a move-only key cannot be moved out of one.
    insert_result insert(value_type&& element)
        requires(!std::move_constructible<value_type>)
                && denied<"insert(std::pair&&): the key is move-only, and a map's element holds it const: move it in, try_emplace(std::move(k), v)", value_type>
    URD_DENY("insert(std::pair&&): the key is move-only, and a map's element holds it const: move it in, try_emplace(std::move(k), v)");

    /// A copy of the key where it is new, by a key with its hash (urd/elements/prehashed.hpp), which is not hashed
    /// again: another form of a key, forwarded as it is (a representation that inserts by none does not compile).
    insert_result insert(const prehashed<value_type>& key)
        requires std::copy_constructible<value_type>
    { return representation().insert(key); }

    insert_result insert(const prehashed<value_type>& key)
        requires denied<"insert(prehashed<T>): the element is move-only: it is copied; move it in, insert(std::move(x))", value_type>
    URD_DENY("insert(prehashed<T>): the element is move-only: it is copied; move it in, insert(std::move(x))");

    /// With a hint where the element goes.
    iterator_type insert(const_iterator_type hint, const value_type& element)
        requires std::copy_constructible<value_type>
    { return representation().insert(hint, element); }

    iterator_type insert(const_iterator_type hint, const value_type& element)
        requires denied<"insert(hint, const T&): the element is move-only: move it in, insert(hint, std::move(x))", value_type>
    URD_DENY("insert(hint, const T&): the element is move-only: move it in, insert(hint, std::move(x))");

    iterator_type insert(const_iterator_type hint, value_type&& element)
        requires std::move_constructible<value_type>
    { return representation().insert(hint, std::move(element)); }

    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
        requires std::copy_constructible<value_type>
    { representation().insert(first, last); }

    template<std::input_iterator InputIt, urd::fixed_string Reason = "insert(first, last): the element is move-only: move each in, insert(std::move(x))">
    void insert(InputIt first, InputIt last)
        requires denied<Reason, value_type>
    URD_DENY("insert(first, last): the element is move-only: move each in, insert(std::move(x))");

    void insert(std::initializer_list<value_type> elements)
        requires std::copy_constructible<value_type>
    { insert(elements.begin(), elements.end()); }

    void insert(std::initializer_list<value_type> elements)
        requires denied<"insert({...}): the element is move-only: an initializer_list is copied; move each in, insert(std::move(x))", value_type>
    URD_DENY("insert({...}): the element is move-only: an initializer_list is copied; move each in, insert(std::move(x))");

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
