#pragma once

#include "urd/interface.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

// Representations of nested Stores, whose elements (rows) are Stores themselves.

namespace urd::repr
{

/// Frozen rows stored flattened, as CSR: offsets into one array of the elements, which at a deeper level are
/// the rows below, stored the same way. A row is a read-only slice of that storage: a std::span of the innermost
/// elements, or a subrange of the rows below. Erase and clear keep the storage.
template<typename Inner>
class Jagged
{
    using Si = container_traits<Inner>;
    using T = typename Si::element_type;
    static constexpr bool leaf = !is_store<T>;
    using Data = std::conditional_t<leaf, std::vector<T>, Jagged<T>>;

public:
    class iterator;
    using const_iterator = iterator;
    using value_type = Inner;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using row = std::conditional_t<leaf, std::span<const T>, std::ranges::subrange<std::ranges::iterator_t<const Data>>>;

    Jagged() = default;

    template<std::input_iterator InputIt>
    Jagged(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& elements) { this->push_back(elements); });
    }

    [[nodiscard]] iterator begin() const { return iterator(this, 0); }
    [[nodiscard]] iterator end() const { return iterator(this, static_cast<difference_type>(size())); }
    [[nodiscard]] size_type size() const { return m_offsets.size() - 1; }

    [[nodiscard]] row operator[](size_type pos) const
    {
        assert(pos < size());
        return row(element(m_offsets[pos]), element(m_offsets[pos + 1]));
    }

    /// Appends a row: the inner Store, or a range of its elements, which are first put in the order the
    /// row's properties ask for.
    template<std::ranges::input_range Row>
    void push_back(const Row& elements)
    { insert(end(), elements); }
    template<std::ranges::input_range Row>
    row emplace_back(const Row& elements)
    {
        push_back(elements);
        return (*this)[size() - 1];
    }
    void pop_back()
    {
        assert(size() > 0);
        erase(end() - 1);
    }
    void clear()
    {
        m_data.clear();
        m_offsets.resize(1);
    }

    template<std::ranges::input_range Row>
    iterator insert(iterator pos, const Row& elements)
    {
        const auto index = static_cast<size_type>(pos - begin());
        const auto before = std::ranges::size(m_data);
        if constexpr (leaf && by_value<Si> && !std::same_as<Row, Inner>)
        {
            const auto normalized = Inner(std::ranges::begin(elements), std::ranges::end(elements));
            insert_elements(m_offsets[index], normalized);
        }
        else
            insert_elements(m_offsets[index], elements);
        const auto added = std::ranges::size(m_data) - before;
        m_offsets.insert(m_offsets.begin() + static_cast<difference_type>(index) + 1, m_offsets[index]);
        for (auto later = index + 1; later < m_offsets.size(); ++later)
            m_offsets[later] += added;
        return begin() + static_cast<difference_type>(index);
    }
    template<std::ranges::input_range Row>
    iterator emplace(iterator pos, const Row& elements)
    { return insert(pos, elements); }

    iterator erase(iterator pos) { return erase(pos, pos + 1); }
    iterator erase(iterator first, iterator last)
    {
        const auto from = static_cast<size_type>(first - begin());
        const auto to = static_cast<size_type>(last - begin());
        const auto removed = m_offsets[to] - m_offsets[from];
        m_data.erase(std::ranges::begin(m_data) + static_cast<difference_type>(m_offsets[from]),
                     std::ranges::begin(m_data) + static_cast<difference_type>(m_offsets[to]));
        m_offsets.erase(m_offsets.begin() + static_cast<difference_type>(from) + 1, m_offsets.begin() + static_cast<difference_type>(to) + 1);
        for (auto later = from + 1; later < m_offsets.size(); ++later)
            m_offsets[later] -= removed;
        return first;
    }

    [[nodiscard]] friend bool operator==(const Jagged& lhs, const Jagged& rhs) = default;

    class iterator
    {
    public:
        using value_type = row;
        using reference = row;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::random_access_iterator_tag;
        using iterator_category = std::input_iterator_tag;

        iterator() = default;
        iterator(const Jagged* rows, difference_type index) : m_rows(rows), m_index(index) {}

        row operator*() const { return (*m_rows)[static_cast<size_type>(m_index)]; }
        row operator[](difference_type n) const { return *(*this + n); }

        iterator& operator++()
        {
            ++m_index;
            return *this;
        }
        iterator operator++(int)
        {
            auto copy = *this;
            ++m_index;
            return copy;
        }
        iterator& operator--()
        {
            --m_index;
            return *this;
        }
        iterator operator--(int)
        {
            auto copy = *this;
            --m_index;
            return copy;
        }
        iterator& operator+=(difference_type n)
        {
            m_index += n;
            return *this;
        }
        iterator& operator-=(difference_type n)
        {
            m_index -= n;
            return *this;
        }
        friend iterator operator+(iterator it, difference_type n) { return it += n; }
        friend iterator operator+(difference_type n, iterator it) { return it += n; }
        friend iterator operator-(iterator it, difference_type n) { return it -= n; }
        friend difference_type operator-(const iterator& lhs, const iterator& rhs) { return lhs.m_index - rhs.m_index; }
        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.m_index == rhs.m_index; }
        friend auto operator<=>(const iterator& lhs, const iterator& rhs) { return lhs.m_index <=> rhs.m_index; }

    private:
        const Jagged* m_rows = nullptr;
        difference_type m_index = 0;
    };

private:
    [[nodiscard]] auto element(size_type pos) const { return std::ranges::begin(m_data) + static_cast<difference_type>(pos); }

    template<typename Row>
    void insert_elements(size_type pos, const Row& elements)
    {
        auto at = std::ranges::begin(m_data) + static_cast<difference_type>(pos);
        if constexpr (leaf)
            m_data.insert(at, std::ranges::begin(elements), std::ranges::end(elements));
        else
            for (const auto& below : elements)
                at = m_data.insert(at, below) + 1;
    }

    std::vector<size_type> m_offsets { 0 };
    Data m_data;
};

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

namespace urd
{

namespace detail
{

/// Whether Jagged can hold rows Inner: frozen rows, of elements or (at a deeper level) of rows in order.
template<typename Inner>
concept flattenable = is_store<Inner> && has<container_traits<Inner>, frozen>
                      && (!is_store<typename container_traits<Inner>::element_type> || !by_value<container_traits<Inner>>);

}  // namespace detail

/// Rows in one array: appends at the end, and erase and clear keep the storage at every level.
template<detail::flattenable Inner>
struct container_traits<repr::Jagged<Inner>> : properties_of<dense, shrinkable, retains, proportional_clear>
{
};

/// The rows are frozen, their elements move when rows before them are inserted or erased, and their storage is
/// kept; innermost rows lie next to each other.
template<detail::flattenable Inner>
struct element_traits<repr::Jagged<Inner>> :
    with_t<container_traits<Inner>, std::conditional_t<is_store<typename container_traits<Inner>::element_type>, any_layout, contiguous>, moving_refs, retains>
{
};

/// Rows that can be cleared are kept for reuse.
template<typename Inner>
    requires(is_store<Inner> && has<container_traits<Inner>, shrinkable>)
struct container_traits<repr::RowPool<Inner>> : properties_of<dense, contiguous, shrinkable, retains, proportional_clear>
{
};

/// The rows keep what they hold when removed: their own properties, memory included.
template<typename Inner>
    requires(is_store<Inner> && has<container_traits<Inner>, shrinkable>)
struct element_traits<repr::RowPool<Inner>> : container_traits<Inner>
{
};

}  // namespace urd
