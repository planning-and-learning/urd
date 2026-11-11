#pragma once

// Rows of unsigned columns, each column in the bits its width gives it: how the element storage of a pool of rows whose
// request promises urd::by_column_widths lays them out (urd/repr/pools/pools.hpp). A row's padding is zero; equal rows
// are hashed and compared by their values, whatever the bits of their columns.

#include "urd/config.hpp"
#include "urd/elements/concepts.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/repr/containers/bit.hpp"
#include "urd/repr/containers/bit_rows.hpp"

#include <algorithm>
#include <bit>
#include <boost/container/small_vector.hpp>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace urd
{

/// The columns of a row, each its bit width (1 to word_bits, urd/config.hpp), with where each starts: what a Store of urd::by_column_widths is
/// built with. A column holds an unsigned value that fits its bits; what the value means (a type, a label) is the
/// user's. Up to 8 columns are held without allocating.
class column_widths
{
public:
    struct field
    {
        std::uint8_t bits;
        std::uint32_t at;  ///< its first bit in the row
    };

    column_widths() = default;
    column_widths(std::initializer_list<std::uint8_t> widths)
    {
        for (const auto bits : widths)
            push_back(bits);
    }
    /// `count` columns of `bits` bits each.
    column_widths(std::size_t count, std::uint8_t bits)
    {
        for (std::size_t i = 0; i < count; ++i)
            push_back(bits);
    }

    /// Appends a column; std::invalid_argument where its bits are not 1 to word_bits.
    void push_back(std::uint8_t bits)
    {
        if (bits == 0 || bits > word_bits)
            throw std::invalid_argument("urd::column_widths: a column takes one bit to a word's bits");
        m_fields.push_back(field { bits, static_cast<std::uint32_t>(m_bits) });
        m_bits += bits;
    }
    void clear() noexcept
    {
        m_fields.clear();
        m_bits = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_fields.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_fields.empty(); }
    /// The bits of column i.
    [[nodiscard]] std::uint8_t operator[](std::size_t i) const noexcept { return m_fields[i].bits; }
    [[nodiscard]] const field& at(std::size_t i) const noexcept
    {
        assert(i < size());
        return m_fields[i];
    }
    /// The bits of a row.
    [[nodiscard]] std::size_t bits() const noexcept { return m_bits; }

    /// The same widths, in the same order.
    friend bool operator==(const column_widths& lhs, const column_widths& rhs) noexcept
    {
        return std::ranges::equal(lhs.m_fields, rhs.m_fields, [](const field& a, const field& b) { return a.bits == b.bits; });
    }

private:
    static constexpr std::size_t inline_columns = 8;  ///< the columns held without allocating

    boost::container::small_vector<field, inline_columns> m_fields;
    std::size_t m_bits = 0;
};

/// Column widths hash by their bits, in order (equal as operator== says).
template<>
struct Hash<column_widths>
{
    hash_t operator()(const column_widths& widths) const noexcept
    {
        return hash_range(std::views::iota(std::size_t { 0 }, widths.size()) | std::views::transform([&widths](std::size_t i) { return widths[i]; }));
    }
};

}  // namespace urd

namespace urd::repr
{

template<bool ThreadSafe>
class SchemaRow;

namespace detail
{

/// A random-access iterator over a row's columns, each read as T (row.get<T>(i)); it holds the row, so it outlives
/// the range it came from.
template<bool ThreadSafe, typename T>
class column_iterator
{
public:
    using value_type = T;
    using reference = T;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept = std::random_access_iterator_tag;

    column_iterator() = default;
    column_iterator(const SchemaRow<ThreadSafe>& row, std::size_t pos) noexcept : m_row(row), m_pos(pos) {}

    reference operator*() const { return m_row.template get<T>(m_pos); }
    reference operator[](difference_type n) const { return *(*this + n); }
    column_iterator& operator++() noexcept { return *this += 1; }
    column_iterator operator++(int) noexcept
    {
        auto copy = *this;
        ++*this;
        return copy;
    }
    column_iterator& operator--() noexcept { return *this -= 1; }
    column_iterator operator--(int) noexcept
    {
        auto copy = *this;
        --*this;
        return copy;
    }
    column_iterator& operator+=(difference_type n) noexcept
    {
        m_pos = static_cast<std::size_t>(static_cast<difference_type>(m_pos) + n);
        return *this;
    }
    column_iterator& operator-=(difference_type n) noexcept { return *this += -n; }
    friend column_iterator operator+(column_iterator it, difference_type n) noexcept { return it += n; }
    friend column_iterator operator+(difference_type n, column_iterator it) noexcept { return it += n; }
    friend column_iterator operator-(column_iterator it, difference_type n) noexcept { return it -= n; }
    friend difference_type operator-(const column_iterator& lhs, const column_iterator& rhs) noexcept
    { return static_cast<difference_type>(lhs.m_pos) - static_cast<difference_type>(rhs.m_pos); }
    friend bool operator==(const column_iterator& lhs, const column_iterator& rhs) noexcept { return lhs.m_pos == rhs.m_pos; }
    friend auto operator<=>(const column_iterator& lhs, const column_iterator& rhs) noexcept { return lhs.m_pos <=> rhs.m_pos; }

private:
    SchemaRow<ThreadSafe> m_row;
    std::size_t m_pos = 0;
};

}  // namespace detail

/// A row of column widths: its columns read on access, as unsigned values (get(i)) or as an index type (get<T>(i)); a
/// borrowed random-access range of its values. Valid while the words it reads and the widths are. With ThreadSafe,
/// read through relaxed atomics, as a writer may append a row sharing a word.
template<bool ThreadSafe>
class SchemaRow
{
    using word = urd::word_t;

public:
    using value_type = word_t;
    using reference = word_t;  ///< a column is decoded on access
    using const_reference = word_t;
    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;
    using const_iterator = detail::column_iterator<ThreadSafe, word_t>;
    using iterator = const_iterator;

    SchemaRow() = default;
    SchemaRow(const word* words, std::size_t first_bit, const urd::column_widths& widths) noexcept : m_words(words), m_first_bit(first_bit), m_widths(&widths)
    {
    }

    [[nodiscard]] const urd::column_widths& column_widths() const noexcept { return *m_widths; }
    [[nodiscard]] size_type size() const noexcept { return m_widths == nullptr ? 0 : m_widths->size(); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    /// Where the row lies: the words and its first bit there (equal for the same stored row).
    [[nodiscard]] const word* data() const noexcept { return m_words; }
    [[nodiscard]] std::size_t first_bit() const noexcept { return m_first_bit; }

    /// The value of column i.
    [[nodiscard]] word_t get(size_type i) const noexcept
    {
        const auto& field = m_widths->at(i);
        return detail::packed_words<ThreadSafe>::get(m_words, m_first_bit + field.at, field.bits);
    }
    /// The value of column i as T: an unsigned integer or an index (index_of).
    template<typename T>
    [[nodiscard]] T get(size_type i) const noexcept
    { return index_of<T>::from_index(static_cast<std::size_t>(get(i))); }
    [[nodiscard]] word_t operator[](size_type i) const noexcept { return get(i); }
    /// Its columns, each read as T (get<T>): a borrowed random-access range.
    template<typename T>
    [[nodiscard]] std::ranges::subrange<detail::column_iterator<ThreadSafe, T>> as() const noexcept
    { return { detail::column_iterator<ThreadSafe, T>(*this, 0), detail::column_iterator<ThreadSafe, T>(*this, size()) }; }

    [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(*this, 0); }
    [[nodiscard]] const_iterator end() const noexcept { return const_iterator(*this, size()); }

private:
    const word* m_words = nullptr;
    std::size_t m_first_bit = 0;
    const urd::column_widths* m_widths = nullptr;
};

/// A row of column widths in words it owns (up to 512 bits without allocating, and reused): what a row is built, looked
/// up and inserted as, written column by column or from a range of values. Valid while the widths are.
template<bool ThreadSafe>
class EncodedRow
{
    using word = urd::word_t;

public:
    EncodedRow() = default;
    explicit EncodedRow(const urd::column_widths& widths) { reset(widths); }

    /// Every column zero, of `widths`.
    void reset(const urd::column_widths& widths)
    {
        m_widths = &widths;
        m_words.assign(bit::packed_blocks<word>(widths.bits()), 0);  // a word of room, as rows have
    }
    /// Column i takes `value`: false where it does not fit the column's bits.
    bool set(std::size_t i, word_t value) noexcept
    {
        const auto& field = m_widths->at(i);
        if (!bit::fits<word>(value, field.bits))
            return false;
        detail::packed_words<false>::set(m_words.data(), field.at, field.bits, value);
        return true;
    }
    /// Column i takes the value of column j of `row`, whose column must be as wide.
    template<bool OtherThreadSafe>
    void copy(std::size_t i, const SchemaRow<OtherThreadSafe>& row, std::size_t j) noexcept
    {
        const auto& field = m_widths->at(i);
        assert(row.column_widths()[j] == field.bits && "urd::repr::EncodedRow::copy: columns alike");
        detail::packed_words<false>::set(m_words.data(), field.at, field.bits, row.get(j));
    }
    /// Every column from `values` (unsigned integers or indices, index_of), in order: false where they are not one
    /// per column or one does not fit. Each column's bits are overwritten, so nothing is zeroed first.
    template<std::ranges::input_range R>
    bool assign(const R& values)
    {
        std::size_t i = 0;
        for (const auto& value : values)
        {
            if (i == m_widths->size() || !set(i, static_cast<word_t>(index_of<std::ranges::range_value_t<R>>::to_index(value))))
                return false;
            ++i;
        }
        return i == m_widths->size();
    }

    [[nodiscard]] SchemaRow<ThreadSafe> row() const noexcept { return SchemaRow<ThreadSafe>(m_words.data(), 0, *m_widths); }

private:
    static constexpr std::size_t inline_words = 9;  ///< the words held without allocating: 512 bits and a word of room

    boost::container::small_vector<word, inline_words> m_words;
    const urd::column_widths* m_widths = nullptr;
};

/// Rows of one set of column widths (urd::column_widths), each its columns' bits back to back; handed out as SchemaRow,
/// taken as rows of equal widths. What a UniformRows of them is laid out by (urd/repr/containers/uniform_rows.hpp): every
/// row has as many columns, so they lie in its segments.
template<bool ThreadSafe>
struct SchemaCodec
{
    using unit = urd::word_t;
    using value_type = SchemaRow<ThreadSafe>;
    using parameter = urd::column_widths;

    explicit SchemaCodec(const urd::column_widths& built_with) : widths(built_with) {}

    /// The words of the rows' bits, and a word of room so that a read of the last column's bits never leaves them.
    [[nodiscard]] std::size_t units(std::size_t rows, std::size_t /*length*/) const noexcept { return bit::packed_blocks<unit>(rows * widths.bits()); }
    [[nodiscard]] value_type read(const unit* data, std::size_t at, std::size_t /*length*/) const noexcept
    { return value_type(data, at * widths.bits(), widths); }
    template<bool OtherThreadSafe>
    void write(unit* data, std::size_t at, const SchemaRow<OtherThreadSafe>& row) const noexcept
    {
        for (std::size_t i = 0; i < widths.size(); ++i)
        {
            const auto& field = widths.at(i);
            detail::packed_words<ThreadSafe>::set(data, (at * widths.bits()) + field.at, field.bits, row.get(i));
        }
    }
    template<bool OtherThreadSafe>
    void check([[maybe_unused]] const SchemaRow<OtherThreadSafe>& row) const noexcept
    { assert(row.column_widths() == widths && "urd::repr::SchemaCodec: a row of its widths"); }

    urd::column_widths widths;
};

}  // namespace urd::repr

template<bool ThreadSafe>
inline constexpr bool std::ranges::enable_borrowed_range<urd::repr::SchemaRow<ThreadSafe>> = true;

/// A row and its columns read as T are values: hashed, compared and ordered by their elements.
template<bool ThreadSafe>
inline constexpr bool urd::value_range<urd::repr::SchemaRow<ThreadSafe>> = true;
template<bool ThreadSafe, typename T>
inline constexpr bool urd::value_range<std::ranges::subrange<urd::repr::detail::column_iterator<ThreadSafe, T>,
                                                             urd::repr::detail::column_iterator<ThreadSafe, T>,
                                                             std::ranges::subrange_kind::sized>> = true;
