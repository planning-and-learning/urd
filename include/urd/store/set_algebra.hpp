#pragma once

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

/// A set: inclusion and overlap with another set of its kind, and union, intersection and difference in place.
/// Each uses the representation's own operation where it has one (a bitset combines whole words), else lookups.
/// The ones that change the set need it to grow (union) or also to shrink (intersection, difference): grows, shrinks.
template<typename Derived, typename R, typename S>
class set_algebra
{
public:
    /// Whether every element of `other` is in this set.
    [[nodiscard]] bool includes(const Derived& other) const
    {
        if constexpr (requires { representation().includes(store_access::representation(other)); })
            return representation().includes(store_access::representation(other));
        else
            return std::ranges::all_of(other, [this](const auto& element) { return self().contains(element); });
    }
    /// Whether this set and `other` share an element.
    [[nodiscard]] bool intersects(const Derived& other) const
    {
        if constexpr (requires { representation().intersects(store_access::representation(other)); })
            return representation().intersects(store_access::representation(other));
        else
            return std::ranges::any_of(other, [this](const auto& element) { return self().contains(element); });
    }

    Derived& operator|=(const Derived& other)
        requires grows<S>
    {
        if constexpr (requires { mutable_representation() |= store_access::representation(other); })
            mutable_representation() |= store_access::representation(other);
        else
            std::ranges::for_each(other, [this](const auto& element) { mutable_self().insert(element); });
        return mutable_self();
    }
    /// Inserts every element of `other`; whether that added any, as fixpoints ask. A union only adds, so the size says.
    bool unite(const Derived& other)
        requires grows<S>
    {
        const auto before = self().size();
        mutable_self() |= other;
        return self().size() != before;
    }
    Derived& operator&=(const Derived& other)
        requires shrinks<S>
    {
        if constexpr (requires { mutable_representation() &= store_access::representation(other); })
            mutable_representation() &= store_access::representation(other);
        else
            erase_if(mutable_self(), [&other](const auto& element) { return !other.contains(element); });
        return mutable_self();
    }
    Derived& operator-=(const Derived& other)
        requires shrinks<S>
    {
        if constexpr (requires { mutable_representation() -= store_access::representation(other); })
            mutable_representation() -= store_access::representation(other);
        else
            erase_if(mutable_self(), [&other](const auto& element) { return other.contains(element); });
        return mutable_self();
    }
    /// As std::set::merge: moves into this set each element of `source` it lacks; `source` keeps the others. With the
    /// representation's own merge where it has one (a node-based set relinks its nodes), else by insert and erase.
    void merge(Derived& source)
        requires grows<S> && shrinks<S>
    {
        if constexpr (requires { mutable_representation().merge(store_access::representation(source)); })
            mutable_representation().merge(store_access::representation(source));
        else
            erase_if(source, [this](const auto& element) { return mutable_self().insert(element).second; });
    }
    void merge(Derived&& source)
        requires grows<S> && shrinks<S>
    { merge(source); }

    // A bitset's words (a dense set), where the representation keeps them: 64 bits each, key k at bit k % 64 of word
    // k / 64.

    /// The words up to the last one with a bit set: equal sets give equal words, however many each keeps.
    [[nodiscard]] std::span<const std::uint64_t> significant_words() const noexcept
        requires requires(const R& held) { held.significant_words(); }
    { return representation().significant_words(); }
    /// Makes this the set whose bits `words` set, its storage reused: a whole assignment, as operator= is (a frozen
    /// set takes it too).
    void assign_words(std::span<const std::uint64_t> words)
        requires requires(R& held, std::span<const std::uint64_t> some) { held.assign_words(some); }
    { mutable_representation().assign_words(words); }
    /// Makes this the set whose bits fill(words) sets in `count` words, zeroed first, written in place.
    template<typename Fill>
        requires std::invocable<Fill&, std::span<std::uint64_t>>
    void assign_words(std::size_t count, Fill fill)
        requires requires(R& held, std::size_t n, Fill& with) { held.assign_words(n, with); }
    { mutable_representation().assign_words(count, std::move(fill)); }

private:
    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
    [[nodiscard]] Derived& mutable_self() { return static_cast<Derived&>(*this); }
    [[nodiscard]] const R& representation() const { return store_access::representation(self()); }
    [[nodiscard]] R& mutable_representation() { return store_access::representation(mutable_self()); }
};

}  // namespace urd::detail::facet
