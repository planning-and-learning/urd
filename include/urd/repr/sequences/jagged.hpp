#pragma once

// Rows of rows: one offsets array per level and the values in one array, rows changed in place.

#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/bitset_span.hpp"
#include "urd/repr/core/position_iterator.hpp"
#include "urd/repr/sequences/rows_fwd.hpp"
#include "urd/repr/values/sparse_set.hpp"
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

/// Rows stored flattened, as CSR: offsets into one array of the elements, which at a deeper level are the rows
/// below, stored the same way. Erase and clear keep the storage.
///
/// Frozen rows are read-only slices of that storage: a std::span of the innermost elements, or a subrange of the
/// rows below. Rows that are not frozen (a fixed_shape Store's) keep their lengths and change in place: a mutable
/// std::span of their elements, or for a dense set a BitsetSpan over its words, a bit per index below its length; one
/// level up, a subrange of such rows, and so on at any depth. reset() sets every element back to its default;
/// with TrackRows, only in the rows handed out to change since the last reset (writes through a row are not seen, so
/// handing it out counts).
/// Declared, with its traits, in urd/repr/sequences/rows_fwd.hpp.

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
    friend struct urd::memory_of<Jagged>;

    using Si = container_traits<Inner>;
    using T = typename Si::element_type;
    static constexpr bool leaf = !is_store<T>;
    static constexpr bool writable = !has<Si, frozen> && (leaf || !has<container_traits<T>, frozen>);
    static constexpr bool bits = writable && leaf && has<Si, unique> && has<Si, dense>;  // a dense set: its bits
    using word = std::uint64_t;
    using Data = std::conditional_t<!leaf, Jagged<T>, std::conditional_t<bits, std::vector<word>, std::vector<T>>>;

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
    using iterator = PositionIterator<std::conditional_t<writable, Jagged, const Jagged>>;
    using const_iterator = PositionIterator<const Jagged>;
    using shape_type = std::conditional_t<leaf, std::vector<size_type>, std::vector<typename shape_below<Data>::type>>;

    Jagged() = default;

    template<std::input_iterator InputIt>
    Jagged(InputIt first, InputIt last)
    { assign(first, last); }

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
    [[nodiscard]] const_iterator end() const { return const_iterator(this, size()); }
    [[nodiscard]] iterator begin()
        requires writable
    { return iterator(this, 0); }
    [[nodiscard]] iterator end()
        requires writable
    { return iterator(this, size()); }
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
        m_lengths.clear();
        if constexpr (TrackRows)
            m_touched.clear();
    }

    template<std::ranges::input_range Row>
    iterator insert(const_iterator pos, const Row& elements)
    {
        const auto index = static_cast<size_type>(pos - begin());
        const auto before = std::ranges::size(m_data);
        if constexpr (leaf && !urd::detail::appends<typename Si::kind> && !bits && !std::same_as<Row, Inner>)
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
    { return insert(pos, elements); }

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
    { return lhs.m_offsets == rhs.m_offsets && lhs.m_data == rhs.m_data && lhs.m_lengths == rhs.m_lengths; }

private:
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

}  // namespace urd::repr

namespace urd
{

template<>
struct include<"urd/repr/sequences/jagged.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<jagged_decl, S>
{
    using type = repr::Jagged<typename S::element_type>;
};
template<typename S>
struct representation_of<fixed_jagged_decl, S>
{
    using type = repr::Jagged<typename S::element_type>;
};
template<typename S>
struct representation_of<tracked_jagged_decl, S>
{
    using type = repr::Jagged<typename S::element_type, true>;
};

}  // namespace urd::detail

/// Its offsets, its elements (or the rows below, or a dense set's words), the lengths and the touched rows.
template<typename Inner, bool TrackRows>
struct urd::memory_of<urd::repr::Jagged<Inner, TrackRows>>
{
    static std::size_t bytes(const repr::Jagged<Inner, TrackRows>& held)
    { return memory_usage(held.m_offsets) + memory_usage(held.m_data) + memory_usage(held.m_lengths) + memory_usage(held.m_touched); }
};
