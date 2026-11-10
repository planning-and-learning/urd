#pragma once

#include "urd/config.hpp"
#include "urd/store/types.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>

namespace urd::detail::facet
{

/// A set: inclusion and overlap with another set of its kind, and union, intersection and difference in place, by
/// whole words: what a set has whose container is dense and canonical (a bitset; by_words). merge every set has.
/// The ones that change the set need it to grow (union) or also to shrink (intersection, difference): grows, shrinks.
template<typename Derived, typename R, typename S>
class set_algebra
{
    /// Whether the container keeps the set as the words of a bitset: its keys dense, equal sets equal words.
    static constexpr bool by_words = has<R, urd::dense, urd::canonical>;

public:
    /// Whether every element of `other` is in this set.
    [[nodiscard]] bool includes(const Derived& other) const
        requires by_words
    { return representation().includes(store_access::representation(other)); }
    /// Whether this set and `other` share an element.
    [[nodiscard]] bool intersects(const Derived& other) const
        requires by_words
    { return representation().intersects(store_access::representation(other)); }

    Derived& operator|=(const Derived& other)
        requires(grows<S> && by_words)
    {
        representation() |= store_access::representation(other);
        return self();
    }
    Derived& operator&=(const Derived& other)
        requires(shrinks<S> && by_words)
    {
        representation() &= store_access::representation(other);
        return self();
    }
    Derived& operator-=(const Derived& other)
        requires(shrinks<S> && by_words)
    {
        representation() -= store_access::representation(other);
        return self();
    }
    /// As std::set::merge: moves into this set each element of `source` it lacks; `source` keeps the others.
    void merge(Derived& source)
        requires grows<S> && shrinks<S>
    { representation().merge(store_access::representation(source)); }
    void merge(Derived&& source)
        requires grows<S> && shrinks<S>
    { merge(source); }

    // A bitset's words (a dense set), where the representation keeps them: word_bits each (urd/config.hpp), key k at bit k % word_bits of word
    // k / word_bits.

    /// The words up to the last one with a bit set: equal sets give equal words, however many each keeps.
    [[nodiscard]] std::span<const word_t> significant_words() const noexcept
        requires by_words
    { return representation().significant_words(); }
    /// Makes this the set whose bits `words` set, its storage reused: a whole assignment, as operator= is (a frozen
    /// set takes it too).
    void assign_words(std::span<const word_t> words)
        requires by_words
    { representation().assign_words(words); }
    /// Makes this the set whose bits fill(words) sets in `count` words, zeroed first, written in place.
    template<typename Fill>
        requires std::invocable<Fill&, std::span<word_t>>
    void assign_words(std::size_t count, Fill fill)
        requires by_words
    { representation().assign_words(count, std::move(fill)); }

private:
    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
    [[nodiscard]] Derived& self() { return static_cast<Derived&>(*this); }
    [[nodiscard]] const R& representation() const { return store_access::representation(self()); }
    [[nodiscard]] R& representation() { return store_access::representation(self()); }
};

}  // namespace urd::detail::facet
