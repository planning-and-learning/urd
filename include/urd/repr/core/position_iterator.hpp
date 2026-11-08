#pragma once

// An iterator by position: what the containers whose elements are read by position (not one range already) iterate
// with.

#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace urd::repr
{

/// Reads an owner's element at a position through its operator[].
struct by_subscript
{
    template<typename Owner>
    decltype(auto) operator()(Owner& owner, std::size_t position) const
    { return owner[position]; }
};

/// The elements of Owner from position 0 to its size, each read as Read {}(owner, position). It is the iterator
/// Concept says (random access by default; a container that is no random-access range, as its declaration says, asks
/// for less), with the operations that concept has. Over a const Owner it is a const_iterator, which the iterator over
/// the Owner converts to. Where reading gives a value (a row's view, an entry), -> points at a copy kept for the
/// expression.
template<typename Owner, typename Read = by_subscript, typename Concept = std::random_access_iterator_tag>
class PositionIterator
{
    static constexpr bool bidirectional = std::derived_from<Concept, std::bidirectional_iterator_tag>;
    static constexpr bool random_access = std::derived_from<Concept, std::random_access_iterator_tag>;

public:
    using reference = decltype(Read {}(std::declval<Owner&>(), std::size_t {}));
    using value_type = std::remove_cvref_t<reference>;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = Concept;
    /// The standard's older algorithms take a value read as an input iterator only.
    using iterator_category = std::conditional_t<std::is_lvalue_reference_v<reference>, Concept, std::input_iterator_tag>;

    /// What -> points at where reading gives a value.
    struct arrow
    {
        value_type value;
        const value_type* operator->() const { return std::addressof(value); }
    };

    PositionIterator() = default;
    PositionIterator(Owner* owner, std::size_t position) noexcept : m_owner(owner), m_position(position) {}
    operator PositionIterator<const Owner, Read, Concept>() const noexcept
        requires(!std::is_const_v<Owner>)
    { return { m_owner, m_position }; }

    reference operator*() const { return Read {}(*m_owner, m_position); }
    auto operator->() const
        requires std::is_lvalue_reference_v<reference>
    { return std::addressof(**this); }
    arrow operator->() const
        requires(!std::is_lvalue_reference_v<reference>)
    { return { **this }; }
    reference operator[](difference_type n) const
        requires random_access
    { return *(*this + n); }

    PositionIterator& operator++() noexcept
    {
        ++m_position;
        return *this;
    }
    PositionIterator operator++(int) noexcept
    {
        auto before = *this;
        ++*this;
        return before;
    }
    PositionIterator& operator--() noexcept
        requires bidirectional
    {
        --m_position;
        return *this;
    }
    PositionIterator operator--(int) noexcept
        requires bidirectional
    {
        auto before = *this;
        --*this;
        return before;
    }
    PositionIterator& operator+=(difference_type n) noexcept
        requires random_access
    {
        m_position = static_cast<std::size_t>(static_cast<difference_type>(m_position) + n);
        return *this;
    }
    PositionIterator& operator-=(difference_type n) noexcept
        requires random_access
    { return *this += -n; }
    friend PositionIterator operator+(PositionIterator it, difference_type n) noexcept
        requires random_access
    { return it += n; }
    friend PositionIterator operator+(difference_type n, PositionIterator it) noexcept
        requires random_access
    { return it += n; }
    friend PositionIterator operator-(PositionIterator it, difference_type n) noexcept
        requires random_access
    { return it -= n; }
    /// How many positions lie between (from bidirectional on: an entry's position is `it - begin()`).
    friend difference_type operator-(const PositionIterator& lhs, const PositionIterator& rhs) noexcept
        requires bidirectional
    { return static_cast<difference_type>(lhs.m_position) - static_cast<difference_type>(rhs.m_position); }
    friend bool operator==(const PositionIterator& lhs, const PositionIterator& rhs) noexcept { return lhs.m_position == rhs.m_position; }
    friend std::strong_ordering operator<=>(const PositionIterator& lhs, const PositionIterator& rhs) noexcept
        requires random_access
    { return lhs.m_position <=> rhs.m_position; }

private:
    Owner* m_owner = nullptr;
    std::size_t m_position = 0;
};

}  // namespace urd::repr
