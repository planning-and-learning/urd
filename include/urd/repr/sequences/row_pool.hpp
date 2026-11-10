#pragma once

// Rows that keep their storage for reuse: a vector of row Stores whose erased rows wait to be refilled.

#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/sequences/rows_fwd.hpp"
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
    /// `count` rows: empty ones, or copies of `row`, appended, or the rows after the first `count` removed (cleared and
    /// kept), as std::vector::resize.
    void resize(size_type count)
    {
        while (m_used > count)
            pop_back();
        while (m_used < count)
            emplace_back();
    }
    void resize(size_type count, const Inner& row)
    {
        while (m_used > count)
            pop_back();
        while (m_used < count)
            push_back(row);
    }
    /// Rebuilds from rows, or as `count` copies of `row`, in the rows kept, as std::vector::assign.
    template<std::input_iterator InputIt>
    void assign(InputIt first, InputIt last)
    {
        clear();
        std::for_each(first, last, [this](const auto& row) { this->push_back(row); });
    }
    void assign(size_type count, const Inner& row)
    {
        clear();
        resize(count, row);
    }
    /// As the vector of its rows: how many fit without growing, the most it can hold, and giving back the rows kept.
    [[nodiscard]] size_type capacity() const { return m_rows.capacity(); }
    [[nodiscard]] size_type max_size() const { return m_rows.max_size(); }
    void shrink_to_fit()
    {
        m_rows.resize(m_used);
        m_rows.shrink_to_fit();
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

public:
    /// The members it owns memory through (urd::memory_of).
    static constexpr auto memory_members() noexcept { return Members<&RowPool::m_rows> {}; }
};

}  // namespace urd::repr

template<> struct urd::include<"urd/repr/sequences/row_pool.hpp"> {};

namespace urd::detail
{

template<typename S>
struct representation_of<row_pool_decl, S>
{
    using type = repr::RowPool<typename S::element_type>;
};

}  // namespace urd::detail

