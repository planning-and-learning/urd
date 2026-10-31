#pragma once

// Rows that keep their storage for reuse: a vector of row Stores whose erased rows wait to be refilled.

#include "urd/elements.hpp"
#include "urd/interface.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/dense.hpp"
#include "urd/repr/rows_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

/// Rows kept for reuse: removing a row clears it instead of destroying it, so its storage serves the next row,
/// and a clear frees nothing at any level.
template<typename Inner>
class RowPool
{
public:
    using value_type = Inner;
    using iterator = typename std::vector<Inner>::iterator;
    using const_iterator = typename std::vector<Inner>::const_iterator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    RowPool() = default;

    template<std::input_iterator InputIt>
    RowPool(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& row) { this->push_back(row); });
    }

    [[nodiscard]] iterator begin() { return m_rows.begin(); }
    [[nodiscard]] iterator end() { return m_rows.begin() + static_cast<difference_type>(m_used); }
    [[nodiscard]] const_iterator begin() const { return m_rows.begin(); }
    [[nodiscard]] const_iterator end() const { return m_rows.begin() + static_cast<difference_type>(m_used); }
    [[nodiscard]] size_type size() const { return m_used; }

    void push_back(const Inner& row) { emplace_back() = row; }
    void push_back(Inner&& row) { emplace_back() = std::move(row); }

    /// The next row: a kept one, cleared, where there is one; built from `args` where given.
    template<typename... Args>
    Inner& emplace_back(Args&&... args)
    {
        if (m_used == m_rows.size())
            m_rows.emplace_back(std::forward<Args>(args)...);
        else if constexpr (sizeof...(Args) > 0)
            m_rows[m_used] = Inner(std::forward<Args>(args)...);
        return m_rows[m_used++];
    }

    void pop_back()
    {
        assert(m_used > 0);
        m_rows[--m_used].clear();
    }
    void clear()
    {
        for (auto& row : *this)
            row.clear();
        m_used = 0;
    }

    iterator insert(const_iterator pos, const Inner& row)
    {
        const auto index = pos - begin();
        push_back(row);
        std::rotate(begin() + index, end() - 1, end());
        return begin() + index;
    }
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args)
    {
        const auto index = pos - begin();
        emplace_back(std::forward<Args>(args)...);
        std::rotate(begin() + index, end() - 1, end());
        return begin() + index;
    }

    iterator erase(const_iterator pos) { return erase(pos, pos + 1); }
    /// Clears the rows and moves them past the rows in use, for reuse.
    iterator erase(const_iterator first, const_iterator last)
    {
        const auto from = first - begin();
        const auto to = last - begin();
        for (auto pos = from; pos < to; ++pos)
            begin()[pos].clear();
        std::rotate(begin() + from, begin() + to, end());
        m_used -= static_cast<size_type>(to - from);
        return begin() + from;
    }

    [[nodiscard]] friend bool operator==(const RowPool& lhs, const RowPool& rhs) { return std::ranges::equal(lhs, rhs); }

private:
    std::vector<Inner> m_rows;  // m_rows[m_used..] are cleared and kept
    size_type m_used = 0;
};

}  // namespace urd::repr
