#pragma once

#include "urd/traits.hpp"

#include <concepts>
#include <cstddef>
#include <ranges>
#include <span>
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
// | shrinkable                       | clear, erase(position) -> (convertible to) iterator, erase(first,  |
// |                                  | last)                                                              |
// | shrinkable, kept by value        | erase(e) -> count                                                  |
// | shrinkable, a sequence           | pop_back, insert(position, e) -> iterator, emplace(position, args) |
// | a map (names a mapped_type)      | the lookups and erase(e) above take a key; with unique keys, at(k) |
// | fifo, lifo, by_priority (queues) | in place of all above: default construction, size, empty, push(e), |
// |                                  | push(e&&), emplace(args), pop; front() for fifo, else top(); clear |
// |                                  | if shrinkable                                                      |
// | handles (pools)                  | in place of all above: insert(span of a row's elements) -> handle, |
// |                                  | get(h) -> row, row_size(h), read(h, span), memory_usage; clear if  |
// |                                  | shrinkable                                                         |
// | growable map with unique keys    | try_emplace(k, v), insert_or_assign(k, v) -> pair<iterator, bool>; |
// |                                  | at(k) -> a mutable value                                           |
//
// "Kept by value" is the standard's associative containers (set, flat_multiset, map, multimap), which place an
// element by its value or key; a sequence (vector) keeps it where it was put.

/// Whether Traits places elements by their value or key, as an associative container does.
template<typename Traits>
inline constexpr bool by_value = has<Traits, sorted> || has<Traits, unique> || has<Traits, multi>;

namespace detail
{

/// What R holds: its value_type where it names one (a container of rows hands out slices of them), else what
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

/// A map: it names what its keys map to.
template<typename R>
concept map_container = requires {
    typename R::mapped_type;
    requires !std::is_void_v<typename R::mapped_type>;
};

/// What R's lookups take: a map's key, else an element.
template<typename R, typename E>
struct lookup_of
{
    using type = E;
};
template<map_container R, typename E>
struct lookup_of<R, E>
{
    using type = typename R::key_type;
};

template<typename R>
concept unique_map_lookup = requires(const R& range, const typename R::key_type& key) {
    { range.at(key) } -> std::same_as<const typename R::mapped_type&>;
};

/// Changing values by key, which a map that can change offers.
template<typename R>
concept unique_map_insertion = requires(R& range, const typename R::key_type& key, const typename R::mapped_type& mapped) {
    { range.at(key) } -> std::same_as<typename R::mapped_type&>;
    { range.try_emplace(key, mapped) } -> std::same_as<std::pair<std::ranges::iterator_t<R>, bool>>;
    { range.insert_or_assign(key, mapped) } -> std::same_as<std::pair<std::ranges::iterator_t<R>, bool>>;
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
    { range.insert(element) } -> std::same_as<std::conditional_t<Unique, std::pair<std::ranges::iterator_t<R>, bool>, std::ranges::iterator_t<R>>>;
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
    // Convertible, as the standard asks: Boost's flat hash tables return a cheaper proxy.
    { range.erase(position) } -> std::convertible_to<std::ranges::iterator_t<R>>;
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

/// A queue: reached by the order its elements leave in, not iterated.
template<typename R, typename E, typename Traits>
concept queue_operations = std::default_initializable<R> && requires(R& queue, const R& constant, const E& element, E&& moved) {
    { constant.size() } -> std::convertible_to<std::size_t>;
    { constant.empty() } -> std::same_as<bool>;
    queue.push(element);
    queue.push(std::move(moved));
    queue.emplace(element);
    queue.pop();
} && (!has<Traits, shrinkable> || requires(R& queue) { queue.clear(); }) && (has<Traits, fifo> ? requires(const R& constant) {
                               { constant.front() } -> std::same_as<const E&>;
                           } : requires(const R& constant) {
                               { constant.top() } -> std::same_as<const E&>;
                           });

/// A pool: rows go in as ranges of their elements and come back by the handle insert gave.
template<typename R, typename Traits>
concept pool_operations =
    std::default_initializable<R>
    && requires(R& pool, const R& constant, const typename R::handle_type& handle, std::span<const std::ranges::range_value_t<typename R::value_type>> elements, std::span<std::ranges::range_value_t<typename R::value_type>> out) {
           { pool.insert(elements) } -> std::same_as<typename R::handle_type>;
           { constant.get(handle) } -> std::same_as<typename R::value_type>;
           { constant.row_size(handle) } -> std::convertible_to<std::size_t>;
           constant.read(handle, out);
           { constant.memory_usage() } -> std::convertible_to<std::size_t>;
           requires std::equality_comparable<typename R::handle_type>;
       } && (!has<Traits, shrinkable> || requires(R& pool) { pool.clear(); });

/// Whether Traits declares an order of leaving: a queue.
template<typename Traits>
inline constexpr bool queue_traits = !std::same_as<category_t<Traits, any_access>, any_access>;

}  // namespace detail

namespace detail
{

// What each capability requires, as the Store's facets group what each offers.

template<typename R, typename Traits, typename L>
concept offers_lookup = (!by_value<Traits> || lookup<R, L>) && (!has<Traits, sorted> || ordered_lookup<R, L>);

template<typename R, typename Traits>
concept offers_values_by_key = !map_container<R> || !has<Traits, unique> || (unique_map_lookup<R> && (!has<Traits, growable> || unique_map_insertion<R>) );

template<typename R, typename Traits, typename E>
concept offers_growth = !has<Traits, growable> || (by_value<Traits> ? value_insertion<R, E, has<Traits, unique>> : sequence_insertion<R, E>);

template<typename R, typename Traits, typename E, typename L>
concept offers_removal = !has<Traits, shrinkable> || (removal<R> && (by_value<Traits> ? value_removal<R, L> : sequence_removal<R, E>) );

template<typename R, typename Traits>
concept offers_layout = (!has<Traits, canonical> || std::equality_comparable<R>) && (!has<Traits, contiguous> || std::ranges::contiguous_range<R>)
                        && (!has<Traits, positional> || std::ranges::random_access_range<R>);

}  // namespace detail

/// Whether R offers what its declared properties require (the table above).
template<typename R,
         typename Traits = container_traits<std::remove_cvref_t<R>>,
         typename E = typename detail::value_of<R>::type,
         typename L = typename detail::lookup_of<R, E>::type>
concept conforming = (detail::queue_traits<Traits> && detail::queue_operations<R, E, Traits>) || (has<Traits, handles> && detail::pool_operations<R, Traits>)
                     || (!detail::queue_traits<Traits> && !has<Traits, handles> && detail::basic_container<R, E> && detail::offers_layout<R, Traits>
                         && detail::offers_lookup<R, Traits, L> && detail::offers_values_by_key<R, Traits> && detail::offers_growth<R, Traits, E>
                         && detail::offers_removal<R, Traits, E, L>);

}  // namespace urd
