#pragma once

#include "urd/interface.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>

namespace urd
{

/// A frozen row of a nested Store, viewing storage the Store owns: the read interface the row's request Si
/// allows. Positions, as for any sequence; lookups where Si keeps elements by value (by binary search where
/// sorted); data() where Si asks for contiguous. At a deeper level its elements are views too. Valid until the
/// Store it views changes.
template<typename Si, std::random_access_iterator It>
class View
{
public:
    using value_type = std::iter_value_t<It>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using const_reference = std::iter_reference_t<It>;
    using reference = const_reference;
    using const_iterator = It;
    using iterator = It;

    View() = default;
    View(It first, It last) : m_first(first), m_last(last) {}

    [[nodiscard]] It begin() const { return m_first; }
    [[nodiscard]] It end() const { return m_last; }
    [[nodiscard]] It cbegin() const { return m_first; }
    [[nodiscard]] It cend() const { return m_last; }
    [[nodiscard]] size_type size() const { return static_cast<size_type>(m_last - m_first); }
    [[nodiscard]] bool empty() const { return m_first == m_last; }

    [[nodiscard]] const_reference operator[](size_type pos) const
    {
        assert(pos < size());
        return m_first[static_cast<difference_type>(pos)];
    }
    [[nodiscard]] const_reference at(size_type pos) const
    {
        if (pos >= size())
            throw std::out_of_range("urd::View::at: position out of range");
        return (*this)[pos];
    }
    [[nodiscard]] const_reference front() const
    {
        assert(!empty());
        return *m_first;
    }
    [[nodiscard]] const_reference back() const
    {
        assert(!empty());
        return *std::prev(m_last);
    }
    [[nodiscard]] auto data() const
        requires(has<Si, contiguous> && std::contiguous_iterator<It>)
    {
        return std::to_address(m_first);
    }

    [[nodiscard]] It find(const value_type& element) const
        requires by_value<Si>
    {
        if constexpr (has<Si, sorted>)
        {
            const auto found = lower_bound(element);
            return found != m_last && *found == element ? found : m_last;
        }
        else
            return std::ranges::find(*this, element);
    }
    [[nodiscard]] size_type count(const value_type& element) const
        requires by_value<Si>
    {
        if constexpr (has<Si, sorted>)
        {
            const auto [first, last] = equal_range(element);
            return static_cast<size_type>(last - first);
        }
        else
            return static_cast<size_type>(std::ranges::count(*this, element));
    }
    [[nodiscard]] bool contains(const value_type& element) const
        requires by_value<Si>
    {
        return find(element) != m_last;
    }
    [[nodiscard]] It lower_bound(const value_type& element) const
        requires has<Si, sorted>
    {
        return std::ranges::lower_bound(*this, element);
    }
    [[nodiscard]] It upper_bound(const value_type& element) const
        requires has<Si, sorted>
    {
        return std::ranges::upper_bound(*this, element);
    }
    [[nodiscard]] std::pair<It, It> equal_range(const value_type& element) const
        requires has<Si, sorted>
    {
        return { lower_bound(element), upper_bound(element) };
    }

    [[nodiscard]] friend bool operator==(const View& lhs, const View& rhs)
        requires std::equality_comparable<value_type>
    {
        return std::ranges::equal(lhs, rhs);
    }
    [[nodiscard]] friend auto operator<=>(const View& lhs, const View& rhs)
        requires std::three_way_comparable<value_type>
    {
        return std::lexicographical_compare_three_way(lhs.m_first, lhs.m_last, rhs.m_first, rhs.m_last);
    }

private:
    It m_first {};
    It m_last {};
};

}  // namespace urd
