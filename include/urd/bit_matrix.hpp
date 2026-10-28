#pragma once

#include "urd/elements.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

namespace urd
{

/// A fixed-size matrix of bits over two dense dimensions (urd::index_of): a relation between rows and columns, such
/// as adjacency or compatibility. Every row has the same length, so the rows lie in one array of words with a fixed
/// stride, and a row is combined with another word by word. Not a Store kind of its own: nested dense sets of one
/// length.
template<typename Row, typename Column = Row>
class BitMatrix
{
    static_assert(indexable<Row> && indexable<Column>, "urd::BitMatrix: rows and columns must be indices (urd::index_of)");

    using word = std::uint64_t;
    static constexpr std::size_t word_bits = std::numeric_limits<word>::digits;

public:
    BitMatrix() = default;
    /// `rows` by `columns` bits, all unset.
    BitMatrix(std::size_t rows, std::size_t columns) :
        m_rows(rows),
        m_columns(columns),
        m_stride((columns + word_bits - 1) / word_bits),
        m_words(rows * m_stride)
    {
    }

    [[nodiscard]] std::size_t rows() const { return m_rows; }
    [[nodiscard]] std::size_t columns() const { return m_columns; }

    [[nodiscard]] bool test(const Row& row, const Column& column) const { return (m_words[at(row, column)] & bit(column)) != 0; }
    void set(const Row& row, const Column& column) { m_words[at(row, column)] |= bit(column); }
    void reset(const Row& row, const Column& column) { m_words[at(row, column)] &= ~bit(column); }
    /// Unsets every bit.
    void clear() { std::ranges::fill(m_words, word { 0 }); }

    /// How many columns of `row` are set.
    [[nodiscard]] std::size_t count(const Row& row) const
    {
        return std::transform_reduce(words(row).begin(),
                                     words(row).end(),
                                     std::size_t { 0 },
                                     std::plus<> {},
                                     [](word each) { return static_cast<std::size_t>(std::popcount(each)); });
    }
    /// Sets in `target` every column set in `source`; true where that set a new one.
    bool unite_rows(const Row& target, const Row& source)
    {
        const auto into = words(target);
        const auto from = std::span<const word>(words(source));
        bool changed = false;
        for (std::size_t each = 0; each < m_stride; ++each)
        {
            const auto united = into[each] | from[each];
            changed = changed || united != into[each];
            into[each] = united;
        }
        return changed;
    }

private:
    [[nodiscard]] static word bit(const Column& column) { return word { 1 } << (index_of<Column>::to_index(column) % word_bits); }
    [[nodiscard]] std::size_t at(const Row& row, const Column& column) const
    {
        assert(index_of<Row>::to_index(row) < m_rows && index_of<Column>::to_index(column) < m_columns);
        return index_of<Row>::to_index(row) * m_stride + index_of<Column>::to_index(column) / word_bits;
    }
    [[nodiscard]] std::span<const word> words(const Row& row) const
    { return std::span<const word>(m_words).subspan(index_of<Row>::to_index(row) * m_stride, m_stride); }
    [[nodiscard]] std::span<word> words(const Row& row) { return std::span<word>(m_words).subspan(index_of<Row>::to_index(row) * m_stride, m_stride); }

    std::size_t m_rows = 0;
    std::size_t m_columns = 0;
    std::size_t m_stride = 0;
    std::vector<word> m_words;
};

}  // namespace urd
