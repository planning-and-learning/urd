#pragma once

#include "urd/operations.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <cstddef>
#include <ranges>
#include <type_traits>
#include <utility>

namespace urd
{

// What a container's properties require it to offer, as Boost.Graph's concepts say what a graph offers: the
// operations of operations.hpp, with the standard library's names and results. A container declares its
// properties in container_traits and must then offer what they require, by itself or through an adapter.
//
// | Property                         | Requires                                                           |
// |----------------------------------|--------------------------------------------------------------------|
// | any                              | a sized forward range, constructible from an iterator range         |
// | canonical                        | ==                                                                 |
// | contiguous                       | a contiguous range                                                 |
// | positional                       | a random-access range                                              |
// | kept by value (sorted or unique) | find, count, contains                                              |
// | sorted                           | a bidirectional range; lower_bound, upper_bound, equal_range       |
// | growable, kept by value          | insert(e) (pair<iterator, bool> if unique, else iterator),         |
// |                                  | insert(e&&), emplace(args), insert(first, last), insert(hint, e)   |
// | growable, a sequence             | push_back(e), push_back(e&&), emplace_back(args) -> reference      |
// | shrinkable                       | clear, erase(position) -> iterator, erase(first, last)             |
// | shrinkable, kept by value        | erase(e) -> count                                                  |
// | shrinkable, a sequence           | pop_back, insert(position, e) -> iterator, emplace(position, args) |
//
// "Kept by value" is the standard's associative containers (set, flat_multiset), which place an element by
// its value; a sequence (vector) keeps it where it was put.

/// Whether Traits places elements by their value, as an associative container does.
template<typename Traits>
inline constexpr bool by_value = has<Traits, sorted> || has<Traits, unique>;

namespace detail
{

template<typename R, typename E>
concept basic_container = std::ranges::forward_range<R> && std::ranges::sized_range<R> && std::constructible_from<R, const E*, const E*>;

template<typename R, typename E>
concept lookup = requires(const R& range, const E& element) {
    { urd::find(range, element) } -> std::same_as<std::ranges::iterator_t<const R>>;
    { urd::count(range, element) } -> std::convertible_to<std::size_t>;
    { urd::contains(range, element) } -> std::same_as<bool>;
};

template<typename R, typename E>
concept ordered_lookup = std::ranges::bidirectional_range<R> && requires(const R& range, const E& element) {
    { urd::lower_bound(range, element) } -> std::same_as<std::ranges::iterator_t<const R>>;
    { urd::upper_bound(range, element) } -> std::same_as<std::ranges::iterator_t<const R>>;
    { urd::equal_range(range, element) } -> std::same_as<std::pair<std::ranges::iterator_t<const R>, std::ranges::iterator_t<const R>>>;
};

template<typename R, typename E, bool Unique>
concept value_insertion = requires(R& range, const E& element, E&& moved, const E* first, std::ranges::iterator_t<const R> hint) {
    { urd::insert(range, element) }
        -> std::same_as<std::conditional_t<Unique, std::pair<std::ranges::iterator_t<R>, bool>, std::ranges::iterator_t<R>>>;
    urd::insert(range, std::move(moved));
    urd::emplace(range, element);
    urd::insert(range, first, first);
    { urd::insert(range, hint, element) } -> std::same_as<std::ranges::iterator_t<R>>;
};

template<typename R, typename E>
concept sequence_insertion = requires(R& range, const E& element, E&& moved) {
    urd::push_back(range, element);
    urd::push_back(range, std::move(moved));
    { urd::emplace_back(range, element) } -> std::same_as<std::ranges::range_reference_t<R>>;
};

template<typename R>
concept removal = requires(R& range, std::ranges::iterator_t<const R> position) {
    urd::clear(range);
    { urd::erase(range, position) } -> std::same_as<std::ranges::iterator_t<R>>;
    urd::erase(range, position, position);
};

template<typename R, typename E>
concept value_removal = requires(R& range, const E& element) {
    { urd::erase(range, element) } -> std::convertible_to<std::size_t>;
};

template<typename R, typename E>
concept sequence_removal = requires(R& range, const E& element, std::ranges::iterator_t<const R> position) {
    urd::pop_back(range);
    { urd::insert(range, position, element) } -> std::same_as<std::ranges::iterator_t<R>>;
    { urd::emplace(range, position, element) } -> std::same_as<std::ranges::iterator_t<R>>;
};

}  // namespace detail

/// Whether R offers what its declared properties require (the table above).
template<typename R, typename Traits = container_traits<std::remove_cvref_t<R>>, typename E = std::ranges::range_value_t<R>>
concept conforming =
    detail::basic_container<R, E> && (!has<Traits, canonical> || std::equality_comparable<R>)
    && (!has<Traits, contiguous> || std::ranges::contiguous_range<R>)
    && (!has<Traits, positional> || std::ranges::random_access_range<R>)
    && (!by_value<Traits> || detail::lookup<R, E>)
    && (!has<Traits, sorted> || detail::ordered_lookup<R, E>)
    && (!has<Traits, growable>
        || (by_value<Traits> && detail::value_insertion<R, E, has<Traits, unique>>)
        || (!by_value<Traits> && detail::sequence_insertion<R, E>))
    && (!has<Traits, shrinkable>
        || (detail::removal<R> && by_value<Traits> && detail::value_removal<R, E>)
        || (detail::removal<R> && !by_value<Traits> && detail::sequence_removal<R, E>));

}  // namespace urd
