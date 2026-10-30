#pragma once

#include "urd/elements.hpp"
#include "urd/interface.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/dense.hpp"
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

// Representations of nested Stores, whose elements (rows) are Stores themselves.

namespace urd::repr
{

/// Rows stored flattened, as CSR: offsets into one array of the elements, which at a deeper level are the rows
/// below, stored the same way. Erase and clear keep the storage.
///
/// Frozen rows are read-only slices of that storage: a std::span of the innermost elements, or a subrange of the
/// rows below. Rows that are not frozen (a fixed_shape Store's) keep their lengths and change in place: a mutable
/// std::span of their elements, or for a dense set a BitsetSpan over its words, a bit per index below its length; one
/// level up, a subrange of such rows, and so on at any depth. reset() sets every element back to its default;
/// with TrackRows, only in the rows handed out to change since the last reset (writes through a row are not seen, so
/// handing it out counts).
template<typename Inner, bool TrackRows = false>
class Jagged;

/// The shape a Jagged of fixed rows is built from: for rows of elements, their lengths; one level up, for each row,
/// the shape of its rows; and so on.
template<typename Data>
struct shape_below
{
    using type = std::vector<std::size_t>;
};
template<typename Inner, bool TrackRows>
struct shape_below<Jagged<Inner, TrackRows>>
{
    using type = typename Jagged<Inner, TrackRows>::shape_type;
};

template<typename Inner, bool TrackRows>
class Jagged
{
    using Si = container_traits<Inner>;
    using T = typename Si::element_type;
    static constexpr bool leaf = !is_store<T>;
    static constexpr bool writable = !has<Si, frozen> && (leaf || !has<container_traits<T>, frozen>);
    static constexpr bool bits = writable && leaf && by_value<Si>;
    using word = std::uint64_t;
    using Data = std::conditional_t<!leaf, Jagged<T>, std::conditional_t<bits, std::vector<word>, std::vector<T>>>;
    template<bool Const>
    class basic_iterator;

public:
    using value_type = Inner;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using const_row = std::
        conditional_t<!leaf, std::ranges::subrange<std::ranges::iterator_t<const Data>>, std::conditional_t<bits, BitsetSpan<const word>, std::span<const T>>>;
    using row = std::conditional_t<
        writable,
        std::conditional_t<!leaf, std::ranges::subrange<std::ranges::iterator_t<Data>>, std::conditional_t<bits, BitsetSpan<word>, std::span<T>>>,
        const_row>;
    using iterator = basic_iterator<!writable>;
    using const_iterator = basic_iterator<true>;
    using shape_type = std::conditional_t<leaf, std::vector<size_type>, std::vector<typename shape_below<Data>::type>>;

    Jagged() = default;

    template<std::input_iterator InputIt>
    Jagged(InputIt first, InputIt last)
    {
        assign(first, last);
    }

    /// Rebuilds from rows, in the storage held: clear keeps it.
    template<std::input_iterator InputIt>
    void assign(InputIt first, InputIt last)
    {
        clear();
        std::for_each(first, last, [this](const auto& elements) { this->push_back(elements); });
    }

    /// Rows of these lengths, of default elements or unset bits.
    explicit Jagged(const std::vector<size_type>& lengths)
        requires writable
    {
        m_offsets.reserve(lengths.size() + 1);
        for (const auto length : lengths)
        {
            m_offsets.push_back(m_offsets.back() + stored(length));
            if constexpr (bits)
                m_lengths.push_back(length);
        }
        m_data.resize(m_offsets.back());
    }
    /// Rows of rows, at any depth: for each row, the shape of its rows.
    explicit Jagged(const shape_type& shape)
        requires(writable && !leaf)
    {
        auto below = typename Data::shape_type();
        m_offsets.reserve(shape.size() + 1);
        for (const auto& rows : shape)
        {
            below.insert(below.end(), rows.begin(), rows.end());
            m_offsets.push_back(m_offsets.back() + rows.size());
        }
        m_data = Data(below);
    }

    /// Every element back to its default, every bit unset; with TrackRows, in the rows handed out to change since the
    /// last reset only.
    void reset()
        requires writable
    {
        if constexpr (TrackRows)
        {
            for (const auto pos : m_touched)
                reset_rows(pos, pos + 1);
            m_touched.clear();
        }
        else
            reset_rows(0, size());
    }
    /// Rows [from, to) back to their defaults.
    void reset_rows(size_type from, size_type to)
        requires writable
    {
        if constexpr (leaf)
            std::fill(std::ranges::begin(m_data) + static_cast<difference_type>(m_offsets[from]),
                      std::ranges::begin(m_data) + static_cast<difference_type>(m_offsets[to]),
                      typename Data::value_type {});
        else
            m_data.reset_rows(m_offsets[from], m_offsets[to]);
    }

    [[nodiscard]] const_iterator begin() const { return const_iterator(this, 0); }
    [[nodiscard]] const_iterator end() const { return const_iterator(this, static_cast<difference_type>(size())); }
    [[nodiscard]] iterator begin()
        requires writable
    {
        return iterator(this, 0);
    }
    [[nodiscard]] iterator end()
        requires writable
    {
        return iterator(this, static_cast<difference_type>(size()));
    }
    [[nodiscard]] size_type size() const { return m_offsets.size() - 1; }

    [[nodiscard]] const_row operator[](size_type pos) const { return row_at<const_row>(*this, pos); }
    [[nodiscard]] row operator[](size_type pos)
        requires writable
    {
        if constexpr (TrackRows)
            m_touched.insert(pos);
        return row_at<row>(*this, pos);
    }

    /// Appends a row: the inner Store, or a range of its elements, which are first put in the order the
    /// row's properties ask for.
    template<std::ranges::input_range Row>
    void push_back(const Row& elements)
    {
        insert(end(), elements);
    }
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
        m_lengths.clear();
        if constexpr (TrackRows)
            m_touched.clear();
    }

    template<std::ranges::input_range Row>
    iterator insert(const_iterator pos, const Row& elements)
    {
        const auto index = static_cast<size_type>(pos - begin());
        const auto before = std::ranges::size(m_data);
        if constexpr (leaf && by_value<Si> && !bits && !std::same_as<Row, Inner>)
        {
            const auto normalized = Inner(std::ranges::begin(elements), std::ranges::end(elements));
            insert_elements(index, normalized);
        }
        else
            insert_elements(index, elements);
        const auto added = std::ranges::size(m_data) - before;
        m_offsets.insert(m_offsets.begin() + static_cast<difference_type>(index) + 1, m_offsets[index]);
        for (auto later = index + 1; later < m_offsets.size(); ++later)
            m_offsets[later] += added;
        return begin() + static_cast<difference_type>(index);
    }
    template<std::ranges::input_range Row>
    iterator emplace(const_iterator pos, const Row& elements)
    {
        return insert(pos, elements);
    }

    iterator erase(const_iterator pos) { return erase(pos, pos + 1); }
    iterator erase(const_iterator first, const_iterator last)
    {
        const auto from = static_cast<size_type>(first - begin());
        const auto to = static_cast<size_type>(last - begin());
        const auto removed = m_offsets[to] - m_offsets[from];
        m_data.erase(std::ranges::begin(m_data) + static_cast<difference_type>(m_offsets[from]),
                     std::ranges::begin(m_data) + static_cast<difference_type>(m_offsets[to]));
        m_offsets.erase(m_offsets.begin() + static_cast<difference_type>(from) + 1, m_offsets.begin() + static_cast<difference_type>(to) + 1);
        for (auto later = from + 1; later < m_offsets.size(); ++later)
            m_offsets[later] -= removed;
        if constexpr (bits)
            m_lengths.erase(m_lengths.begin() + static_cast<difference_type>(from), m_lengths.begin() + static_cast<difference_type>(to));
        return begin() + static_cast<difference_type>(from);
    }

    /// Equal rows; bits past a row's length are always unset, so for bits the words say it. Which rows were handed out
    /// is not part of the contents.
    [[nodiscard]] friend bool operator==(const Jagged& lhs, const Jagged& rhs)
    {
        return lhs.m_offsets == rhs.m_offsets && lhs.m_data == rhs.m_data && lhs.m_lengths == rhs.m_lengths;
    }

private:
    template<bool Const>
    class basic_iterator
    {
        using Rows = std::conditional_t<Const, const Jagged, Jagged>;

    public:
        // A row as the slice or span it is handed out as.
        using value_type = std::conditional_t<Const, const_row, row>;
        using reference = value_type;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::random_access_iterator_tag;
        using iterator_category = std::input_iterator_tag;

        basic_iterator() = default;
        basic_iterator(Rows* rows, difference_type index) : m_rows(rows), m_index(index) {}
        /// Rows to change convert to rows to read.
        operator basic_iterator<true>() const
            requires(!Const)
        {
            return { m_rows, m_index };
        }

        reference operator*() const { return (*m_rows)[static_cast<size_type>(m_index)]; }
        reference operator[](difference_type n) const { return *(*this + n); }

        basic_iterator& operator++()
        {
            ++m_index;
            return *this;
        }
        basic_iterator operator++(int)
        {
            auto copy = *this;
            ++m_index;
            return copy;
        }
        basic_iterator& operator--()
        {
            --m_index;
            return *this;
        }
        basic_iterator operator--(int)
        {
            auto copy = *this;
            --m_index;
            return copy;
        }
        basic_iterator& operator+=(difference_type n)
        {
            m_index += n;
            return *this;
        }
        basic_iterator& operator-=(difference_type n)
        {
            m_index -= n;
            return *this;
        }
        friend basic_iterator operator+(basic_iterator it, difference_type n) { return it += n; }
        friend basic_iterator operator+(difference_type n, basic_iterator it) { return it += n; }
        friend basic_iterator operator-(basic_iterator it, difference_type n) { return it -= n; }
        friend difference_type operator-(const basic_iterator& lhs, const basic_iterator& rhs) { return lhs.m_index - rhs.m_index; }
        friend bool operator==(const basic_iterator& lhs, const basic_iterator& rhs) { return lhs.m_index == rhs.m_index; }
        friend auto operator<=>(const basic_iterator& lhs, const basic_iterator& rhs) { return lhs.m_index <=> rhs.m_index; }

    private:
        Rows* m_rows = nullptr;
        difference_type m_index = 0;
    };

    /// What a row of `length` elements takes of the storage: as many, or for bits the words that hold them.
    [[nodiscard]] static size_type stored(size_type length) { return bits ? BitsetSpan<word>::num_blocks(length) : length; }

    template<typename Row, typename Self>
    [[nodiscard]] static Row row_at(Self& self, size_type pos)
    {
        assert(pos < self.size());
        const auto element = [&self](size_type at) { return std::ranges::begin(self.m_data) + static_cast<difference_type>(at); };
        if constexpr (bits)
            return Row(std::span(self.m_data).subspan(self.m_offsets[pos]).data(), self.m_lengths[pos]);
        else
            return Row(element(self.m_offsets[pos]), element(self.m_offsets[pos + 1]));
    }

    template<typename Row>
    void insert_elements(size_type index, const Row& elements)
    {
        auto at = std::ranges::begin(m_data) + static_cast<difference_type>(m_offsets[index]);
        if constexpr (bits)
        {
            auto indices = std::vector<size_type>();
            for (const auto& element : elements)
                indices.push_back(index_of<typename Si::key_type>::to_index(element));
            const auto length = indices.empty() ? 0 : std::ranges::max(indices) + 1;
            at = m_data.insert(at, stored(length), word { 0 });
            auto row_bits = BitsetSpan<word>(std::to_address(at), length);
            for (const auto each : indices)
                row_bits.set(each);
            m_lengths.insert(m_lengths.begin() + static_cast<difference_type>(index), length);
        }
        else if constexpr (leaf)
            m_data.insert(at, std::ranges::begin(elements), std::ranges::end(elements));
        else
            for (const auto& below : elements)
                at = m_data.insert(at, below) + 1;
    }

    std::vector<size_type> m_offsets { 0 };
    Data m_data;
    /// The rows' lengths in bits, where they are bits; else always empty.
    std::vector<size_type> m_lengths;
    struct NoRows
    {
    };
    /// The rows handed out to change since the last reset, with TrackRows.
    [[no_unique_address]] std::conditional_t<TrackRows, SparseSet<size_type>, NoRows> m_touched;
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

/// Whether Jagged can hold rows Inner that change in place: rows of elements, as a sequence, or a dense set's bits;
/// or fixed-shape rows of such rows, at any depth.
template<typename Inner>
consteval bool holds_fixed_rows()
{
    if constexpr (!is_store<Inner>)
        return false;
    else
    {
        using traits = container_traits<Inner>;
        using below = typename traits::element_type;
        if constexpr (has<traits, frozen>)
            return false;
        else if constexpr (!is_store<below>)
            return !by_value<traits> || (traits::is_set && has<traits, unique, dense>);
        else
            return has<traits, fixed_shape> && holds_fixed_rows<below>();
    }
}
template<typename Inner>
concept fixed_rows = holds_fixed_rows<Inner>();

}  // namespace detail

/// Rows in one array: appends at the end, and erase and clear keep the storage at every level.
template<detail::flattenable Inner>
struct container_traits<repr::Jagged<Inner>> : properties_of<dense, positional, shrinkable, retains, proportional_clear>
{
};

/// The rows are frozen, their elements move when rows before them are inserted or erased, and their storage is
/// kept; innermost rows lie next to each other.
template<detail::flattenable Inner>
struct element_traits<repr::Jagged<Inner>> :
    with_t<container_traits<Inner>, std::conditional_t<is_store<typename container_traits<Inner>::element_type>, any_layout, contiguous>, moving_refs, retains>
{
};

/// Rows of fixed lengths, changed in place; with TrackRows, reset costs what the rows handed out since hold.
template<detail::fixed_rows Inner, bool TrackRows>
struct container_traits<repr::Jagged<Inner, TrackRows>> :
    properties_of<dense, positional, fixed_shape, retains, std::conditional_t<TrackRows, proportional_clear, any_clear>>
{
};

/// The rows keep their lengths and their storage; a sequence's elements lie next to each other.
template<detail::fixed_rows Inner, bool TrackRows>
struct element_traits<repr::Jagged<Inner, TrackRows>> :
    with_t<container_traits<Inner>,
           std::conditional_t<!is_store<typename container_traits<Inner>::element_type> && !by_value<container_traits<Inner>>, contiguous, any_layout>,
           moving_refs,
           retains>
{
};

/// Rows that can be cleared are kept for reuse.
template<typename Inner>
    requires(is_store<Inner> && has<container_traits<Inner>, shrinkable>)
struct container_traits<repr::RowPool<Inner>> : properties_of<dense, positional, contiguous, shrinkable, retains, proportional_clear>
{
};

/// The rows keep what they hold when removed: their own properties, memory included.
template<typename Inner>
    requires(is_store<Inner> && has<container_traits<Inner>, shrinkable>)
struct element_traits<repr::RowPool<Inner>> : container_traits<Inner>
{
};

}  // namespace urd
