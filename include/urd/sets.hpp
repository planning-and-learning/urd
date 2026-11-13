#pragma once

// Operations on any set, written once from its primitives, which a set that does better overrides (the brokkr coding
// rule "Derived operations"): the default, and an overload constrained by what promises better. A dense set (a bitset)
// combines a word at a time inside urd, so that no public interface hands out bits.

#include "urd/repr/values/dense_set.hpp"
#include "urd/store.hpp"
#include "urd/tags.hpp"

#include <concepts>
#include <ranges>
#include <utility>

namespace urd
{

/// A set to combine: membership, insertion, clearing, and its elements in a range.
template<typename Set>
concept combinable_set = requires(Set& set, const Set& read, std::ranges::range_value_t<Set> key) {
    set.insert(key);
    set.clear();
    { read.contains(key) } -> std::convertible_to<bool>;
};

namespace detail
{

/// The default: the elements of `lhs` that `rhs` holds, one at a time.
template<combinable_set Set>
void set_intersection_by_iteration(const Set& lhs, const Set& rhs, Set& into)
{
    if (&into == &lhs || &into == &rhs)
    {
        auto result = Set(lhs);
        set_intersection_by_iteration(lhs, rhs, result);
        into = std::move(result);
        return;
    }
    into.clear();
    for (const auto& key : lhs)
        if (rhs.contains(key))
            into.insert(key);
}

/// Whether Set keeps the words of a bitset: then sets combine a word at a time.
template<typename Set>
concept set_by_words = is_store<Set> && has<typename Set::representation, dense, canonical>;

}  // namespace detail

/// Sets `into` to the elements `lhs` and `rhs` share, its storage reused: the intersection without a copy first.
/// `into` may be `lhs` or `rhs`.
template<combinable_set Set>
void set_intersection(const Set& lhs, const Set& rhs, Set& into)
{ detail::set_intersection_by_iteration(lhs, rhs, into); }
template<combinable_set Set>
    requires detail::set_by_words<Set>
void set_intersection(const Set& lhs, const Set& rhs, Set& into)
{
    detail::dense_words::assign_intersection(detail::store_access::representation(into),
                                             detail::store_access::representation(lhs),
                                             detail::store_access::representation(rhs));
}

}  // namespace urd
