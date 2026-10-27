#pragma once

#include "urd/traits.hpp"

#include <concepts>
#include <cstddef>
#include <ranges>
#include <type_traits>
#include <utility>

namespace urd
{

// What a container's properties require it to offer, as Boost.Graph's concepts say what a graph offers: member
// functions with the standard library's names and results. A container declares its properties in
// container_traits and must then offer what they require. A library with other names gets an adapter: a class
// that wraps it and renames, adding no guarantee.
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

/// What R holds: its value_type where it names one (a container of rows hands out views of them), else what
/// iterating it gives.
template<typename R>
struct value_of
{
    using type = std::ranges::range_value_t<R>;
};
template<typename R>
    requires requires { typename R::value_type; }
struct value_of<R>
{
    using type = typename R::value_type;
};

template<typename R, typename E>
concept basic_container = std::ranges::forward_range<R> && std::ranges::sized_range<R> && std::constructible_from<R, const E*, const E*>;

template<typename R, typename E>
concept lookup = requires(const R& range, const E& element) {
    { range.find(element) } -> std::same_as<std::ranges::iterator_t<const R>>;
    { range.count(element) } -> std::convertible_to<std::size_t>;
    { range.contains(element) } -> std::same_as<bool>;
};

template<typename R, typename E>
concept ordered_lookup = std::ranges::bidirectional_range<R> && requires(const R& range, const E& element) {
    { range.lower_bound(element) } -> std::same_as<std::ranges::iterator_t<const R>>;
    { range.upper_bound(element) } -> std::same_as<std::ranges::iterator_t<const R>>;
    { range.equal_range(element) } -> std::same_as<std::pair<std::ranges::iterator_t<const R>, std::ranges::iterator_t<const R>>>;
};

template<typename R, typename E, bool Unique>
concept value_insertion = requires(R& range, const E& element, E&& moved, const E* first, std::ranges::iterator_t<const R> hint) {
    { range.insert(element) }
        -> std::same_as<std::conditional_t<Unique, std::pair<std::ranges::iterator_t<R>, bool>, std::ranges::iterator_t<R>>>;
    range.insert(std::move(moved));
    range.emplace(element);
    range.insert(first, first);
    { range.insert(hint, element) } -> std::same_as<std::ranges::iterator_t<R>>;
};

template<typename R, typename E>
concept sequence_insertion = requires(R& range, const E& element, E&& moved) {
    range.push_back(element);
    range.push_back(std::move(moved));
    { range.emplace_back(element) } -> std::same_as<std::ranges::range_reference_t<R>>;
};

template<typename R>
concept removal = requires(R& range, std::ranges::iterator_t<const R> position) {
    range.clear();
    { range.erase(position) } -> std::same_as<std::ranges::iterator_t<R>>;
    range.erase(position, position);
};

template<typename R, typename E>
concept value_removal = requires(R& range, const E& element) {
    { range.erase(element) } -> std::convertible_to<std::size_t>;
};

template<typename R, typename E>
concept sequence_removal = requires(R& range, const E& element, std::ranges::iterator_t<const R> position) {
    range.pop_back();
    { range.insert(position, element) } -> std::same_as<std::ranges::iterator_t<R>>;
    { range.emplace(position, element) } -> std::same_as<std::ranges::iterator_t<R>>;
};

}  // namespace detail

/// Whether R offers what its declared properties require (the table above).
template<typename R, typename Traits = container_traits<std::remove_cvref_t<R>>, typename E = typename detail::value_of<R>::type>
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
