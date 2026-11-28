#pragma once

// A record's content read through its views: hashed and compared by the values its identifying members read as, each
// read through the view (a child as the view of the record it names, read so in turn), not by the view's identity. So
// the forms of one kind (a builder, a record held in place, an interned record) with the same content hash and compare
// alike, read through any contexts.

#include "urd/elements/child_view.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/view_decl.hpp"

#include <boost/container_hash/hash.hpp>
#include <cstddef>
#include <ranges>
#include <type_traits>
#include <variant>

namespace urd
{

namespace detail
{
/// Whether R reads a record by its views: a view, a child view, or a resolved child (the variant of the views of its
/// forms): what urd::visit_view takes.
template<typename R>
inline constexpr bool is_reading = View<R>;
template<View... Vs>
inline constexpr bool is_reading<std::variant<Vs...>> = true;
template<typename T, typename... Vs>
inline constexpr bool is_reading<child_view<T, Vs...>> = true;

template<typename R>
concept reading = is_reading<std::remove_cvref_t<R>>;

/// Whether R is a range of readings (a member of children, resolved).
template<typename R>
concept reading_range = std::ranges::input_range<R> && reading<std::ranges::range_value_t<R>>;
}  // namespace detail

template<View V>
hash_t hash_content(const V& view);

template<View V, View W>
    requires std::same_as<view_kind_t<V>, view_kind_t<W>>
bool equal_content(const V& lhs, const W& rhs);

namespace detail
{
/// Mixes a hash into `seed`, as urd::hash_combine mixes a value's.
inline void combine_hash(hash_t& seed, hash_t hash)
{
    auto combined = static_cast<std::size_t>(seed);
    boost::hash_combine(combined, hash);
    seed = combined;
}

/// The content hash of what a member reads as: a reading's the content of the view of its form, a range of readings'
/// its elements' in order (as urd::hash_range mixes them), any other value its Hash.
template<typename R>
hash_t hash_reading(const R& read)
{
    if constexpr (reading<R>)
        return urd::visit_view(read, [](const auto& view) { return urd::hash_content(view); });
    else if constexpr (reading_range<R>)
    {
        hash_t seed = 0;
        for (const auto& element : read)
            detail::combine_hash(seed, detail::hash_reading(element));
        return seed;
    }
    else
        return Hash<std::remove_cvref_t<R>> {}(read);
}

/// The content hash of a view's members Ms (its record form's identifying members), combined as Hash combines an
/// identified type's (detail::hash_members): one member its own, several mixed into a seed of their count.
template<typename V, auto... Ms>
hash_t hash_read_members(const V& view, Members<Ms...> /*members*/)
{
    if constexpr (sizeof...(Ms) == 1)
        return (detail::hash_reading(view.template member<Ms>()), ...);
    else
    {
        hash_t seed = sizeof...(Ms);
        (detail::combine_hash(seed, detail::hash_reading(view.template member<Ms>())), ...);
        return seed;
    }
}

template<typename L, typename R>
bool equal_reading(const L& lhs, const R& rhs);

/// Whether two ranges hold equal content, element by element, in order.
template<typename L, typename R>
bool equal_ranges(const L& lhs, const R& rhs)
{
    auto left = std::ranges::begin(lhs);
    auto right = std::ranges::begin(rhs);
    for (; left != std::ranges::end(lhs) && right != std::ranges::end(rhs); ++left, ++right)
        if (!detail::equal_reading(*left, *right))
            return false;
    return left == std::ranges::end(lhs) && right == std::ranges::end(rhs);
}

/// Whether what two members read as have equal content: two readings by the views of their forms, two ranges of
/// readings (or of values of different types) element by element, values of one type by their EqualTo, others by
/// EqualTo<>.
template<typename L, typename R>
bool equal_reading(const L& lhs, const R& rhs)
{
    if constexpr (reading<L> && reading<R>)
        return urd::visit_view(lhs,
                               [&rhs](const auto& left)
                               { return urd::visit_view(rhs, [&left](const auto& right) { return urd::equal_content(left, right); }); });
    else if constexpr (std::same_as<std::remove_cvref_t<L>, std::remove_cvref_t<R>> && !reading_range<L>)
        return EqualTo<std::remove_cvref_t<L>> {}(lhs, rhs);
    else if constexpr (std::ranges::input_range<L> && std::ranges::input_range<R>)
        return detail::equal_ranges(lhs, rhs);
    else
        return EqualTo<> {}(lhs, rhs);
}

/// Whether two views' members Ms and Ns (their record forms' identifying members, by position) have equal content.
template<typename V, typename W, auto... Ms, auto... Ns>
bool equal_read_members(const V& lhs, const W& rhs, Members<Ms...> /*left*/, Members<Ns...> /*right*/)
{
    static_assert(sizeof...(Ms) == sizeof...(Ns), "urd::equal_content: the forms of a kind have its identifying members, in order");
    return (detail::equal_reading(lhs.template member<Ms>(), rhs.template member<Ns>()) && ...);
}
}  // namespace detail

/// The hash of what a view reads: its record form's identifying members, each read through the view (a child as the
/// content of the record it names), combined as urd::Hash combines them. A builder's view hashes as Hash hashes the
/// builder where each identifying member hashes so too (a value; a builder held in place, read as its view), not where
/// one is a child (Hash takes a child by its form); the record a builder interns to, held in place or interned, hashes
/// as the builder's view does, read through any context.
template<View V>
hash_t hash_content(const V& view)
{ return detail::hash_read_members(view, urd::identifying_members_of<record_of<V>>()); }

/// Whether two views of one kind read equal content (hash_content's): their record forms' identifying members, by
/// position, read through each view.
template<View V, View W>
    requires std::same_as<view_kind_t<V>, view_kind_t<W>>
bool equal_content(const V& lhs, const W& rhs)
{ return detail::equal_read_members(lhs, rhs, urd::identifying_members_of<record_of<V>>(), urd::identifying_members_of<record_of<W>>()); }

}  // namespace urd
