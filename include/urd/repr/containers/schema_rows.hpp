#pragma once

// Rows of unsigned columns, each column in the bits its width gives it, in segments that never move: the element storage
// of a pool of rows whose request promises urd::by_column_widths (urd/repr/pools/pools.hpp). A row's padding is zero;
// equal rows are hashed and compared by their values, whatever the bits of their columns.

#include "urd/elements/concepts.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/repr/containers/bit.hpp"
#include "urd/repr/containers/bit_rows.hpp"
#include "urd/repr/containers/detail/geometric_segment_layout.hpp"
#include "urd/repr/containers/detail/threading.hpp"
#include "urd/repr/containers/segment_sizes.hpp"
#include "urd/repr/core/position_iterator.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <boost/container/small_vector.hpp>
#include <boost/hash2/xxhash.hpp>
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

/// The columns of a row, each its bit width (1 to 64), with where each starts: what a Store of urd::by_column_widths is
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

    /// Appends a column; std::invalid_argument where its bits are not 1 to 64.
    void push_back(std::uint8_t bits)
    {
        if (bits == 0 || bits > 64)
            throw std::invalid_argument("urd::column_widths: a column takes 1 to 64 bits");
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
    { return std::ranges::equal(lhs.m_fields, rhs.m_fields, [](const field& a, const field& b) { return a.bits == b.bits; }); }

private:
    boost::container::small_vector<field, 8> m_fields;
    std::size_t m_bits = 0;
};

/// The hash of a row whose columns hold these values (value(i) for i below count): alike for a stored row and for the
/// values it was made from, whatever the bits of its columns.
template<typename Value>
[[nodiscard]] hash_t hash_columns(std::size_t count, Value value) noexcept
{
    auto hash = boost::hash2::xxhash_64 {};
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint64_t each = value(i);
        hash.update(&each, sizeof(each));
    }
    return hash.result();
}
/// The hash of the row made from these values, unsigned integers or indices (index_of; see hash_columns).
template<std::ranges::sized_range R>
[[nodiscard]] hash_t hash_columns(const R& values) noexcept
{
    auto it = std::ranges::begin(values);
    return hash_columns(std::ranges::size(values),
                        [&it](std::size_t) { return static_cast<std::uint64_t>(index_of<std::ranges::range_value_t<R>>::to_index(*it++)); });
}

/// Column widths hash by their bits, in order (equal as operator== says).
template<>
struct Hash<column_widths>
{
    hash_t operator()(const column_widths& widths) const noexcept
    { return hash_columns(widths.size(), [&widths](std::size_t i) { return widths[i]; }); }
};

}  // namespace urd

namespace urd::repr
{

template<bool ThreadSafe>
class SchemaRow;

namespace detail
{

/// Whether a value fits a column of `bits` bits.
[[nodiscard]] constexpr bool fits(std::uint8_t bits, std::uint64_t value) noexcept { return bits == 64 || (value >> bits) == 0; }

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
    using word = std::uint64_t;

public:
    using value_type = std::uint64_t;
    using size_type = std::size_t;
    using const_iterator = detail::column_iterator<ThreadSafe, std::uint64_t>;
    using iterator = const_iterator;

    SchemaRow() = default;
    SchemaRow(const word* words, std::size_t first_bit, const urd::column_widths& widths) noexcept :
        m_words(words),
        m_first_bit(first_bit),
        m_widths(&widths)
    {
    }

    [[nodiscard]] const urd::column_widths& column_widths() const noexcept { return *m_widths; }
    [[nodiscard]] size_type size() const noexcept { return m_widths == nullptr ? 0 : m_widths->size(); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    /// Where the row lies: the words and its first bit there (equal for the same stored row).
    [[nodiscard]] const word* data() const noexcept { return m_words; }
    [[nodiscard]] std::size_t first_bit() const noexcept { return m_first_bit; }

    /// The value of column i.
    [[nodiscard]] std::uint64_t get(size_type i) const noexcept
    {
        const auto& field = m_widths->at(i);
        return detail::packed_words<ThreadSafe>::get(m_words, m_first_bit + field.at, field.bits);
    }
    /// The value of column i as T: an unsigned integer or an index (index_of).
    template<typename T>
    [[nodiscard]] T get(size_type i) const noexcept
    { return index_of<T>::from_index(static_cast<std::size_t>(get(i))); }
    [[nodiscard]] std::uint64_t operator[](size_type i) const noexcept { return get(i); }
    /// Its columns, each read as T (get<T>): a borrowed random-access range.
    template<typename T>
    [[nodiscard]] std::ranges::subrange<detail::column_iterator<ThreadSafe, T>> as() const noexcept
    { return { detail::column_iterator<ThreadSafe, T>(*this, 0), detail::column_iterator<ThreadSafe, T>(*this, size()) }; }

    [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(*this, 0); }
    [[nodiscard]] const_iterator end() const noexcept { return const_iterator(*this, size()); }

    /// Hashed by its values (urd::hash_columns).
    [[nodiscard]] hash_t hash() const noexcept
    { return hash_columns(size(), [this](size_type i) { return get(i); }); }
    /// Equal values, column by column, whatever the bits of the columns.
    template<bool OtherThreadSafe>
    [[nodiscard]] bool equal(const SchemaRow<OtherThreadSafe>& other) const noexcept
    {
        if (size() != other.size())
            return false;
        for (size_type i = 0; i < size(); ++i)
            if (get(i) != other.get(i))
                return false;
        return true;
    }

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
    using word = std::uint64_t;

public:
    EncodedRow() = default;
    explicit EncodedRow(const urd::column_widths& widths) { reset(widths); }

    /// Every column zero, of `widths`.
    void reset(const urd::column_widths& widths)
    {
        m_widths = &widths;
        m_words.assign(bit::ceil_div<std::size_t>(widths.bits(), 64) + 1, 0);  // a word of room, as rows have
    }
    /// Column i takes `value`: false where it does not fit the column's bits.
    bool set(std::size_t i, std::uint64_t value) noexcept
    {
        const auto& field = m_widths->at(i);
        if (!detail::fits(field.bits, value))
            return false;
        bit::write_int<word>(std::span<word>(m_words.data() + (field.at / 64), 2), value, static_cast<std::uint8_t>(field.at % 64), field.bits);
        return true;
    }
    /// Column i takes the value of column j of `row`, whose column must be as wide.
    template<bool OtherThreadSafe>
    void copy(std::size_t i, const SchemaRow<OtherThreadSafe>& row, std::size_t j) noexcept
    {
        const auto& field = m_widths->at(i);
        assert(row.column_widths()[j] == field.bits && "urd::repr::EncodedRow::copy: columns alike");
        bit::write_int<word>(std::span<word>(m_words.data() + (field.at / 64), 2), row.get(j), static_cast<std::uint8_t>(field.at % 64), field.bits);
    }
    /// Every column from `values` (unsigned integers or indices, index_of), in order: false where they are not one
    /// per column or one does not fit.
    template<std::ranges::input_range R>
    bool assign(const R& values)
    {
        std::fill(m_words.begin(), m_words.end(), word { 0 });
        std::size_t i = 0;
        for (const auto& value : values)
            if (i == m_widths->size() || !set(i++, static_cast<std::uint64_t>(index_of<std::ranges::range_value_t<R>>::to_index(value))))
                return false;
        return i == m_widths->size();
    }

    [[nodiscard]] SchemaRow<ThreadSafe> row() const noexcept { return SchemaRow<ThreadSafe>(m_words.data(), 0, *m_widths); }

private:
    boost::container::small_vector<word, 9> m_words;
    const urd::column_widths* m_widths = nullptr;
};

/// Rows of one set of column widths (urd::column_widths), each its columns' bits back to back: row i lies at bit
/// i * widths.bits() of segments of geometrically growing size (in rows), which never move. Hands out rows as
/// SchemaRow; takes rows of equal widths. With ThreadSafe, rows below size() may be read while one thread (under the owner's lock) appends.
/// Clear keeps the segments.
template<bool ThreadSafe>
class SchemaRows
{
    friend struct urd::memory_of<SchemaRows>;

    using word = std::uint64_t;
    using words = detail::packed_words<ThreadSafe>;
    using Layout = detail::GeometricSegmentLayout<default_first_array_segment_size>;

public:
    /// What it is built with.
    using parameter = urd::column_widths;
    using value_type = SchemaRow<ThreadSafe>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using const_iterator = PositionIterator<const SchemaRows>;
    using iterator = const_iterator;

    explicit SchemaRows(const urd::column_widths& widths) : m_widths(widths) {}
    SchemaRows(const SchemaRows&) = delete;
    SchemaRows& operator=(const SchemaRows&) = delete;
    SchemaRows(SchemaRows&&) = delete;
    SchemaRows& operator=(SchemaRows&&) = delete;
    ~SchemaRows()
    {
        for (auto& segment : m_segments)
            delete[] repr::detail::load_pointer<ThreadSafe, word>(segment);
    }

    [[nodiscard]] const urd::column_widths& column_widths() const noexcept { return m_widths; }
    [[nodiscard]] size_type size() const noexcept { return repr::detail::load_size<ThreadSafe>(m_size); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    [[nodiscard]] const_iterator begin() const { return const_iterator(this, 0); }
    [[nodiscard]] const_iterator end() const { return const_iterator(this, size()); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend() const { return end(); }

    [[nodiscard]] value_type operator[](size_type pos) const noexcept
    {
        const auto segment = Layout::segment_index(pos);
        return value_type(repr::detail::load_pointer<ThreadSafe, word>(m_segments[segment]), Layout::segment_offset(pos, segment) * m_widths.bits(), m_widths);
    }

    /// Appends a row of equal widths: by one writer at a time (the owner's lock).
    template<bool OtherThreadSafe>
    void push_back(const SchemaRow<OtherThreadSafe>& row)
    {
        assert(row.column_widths() == m_widths && "urd::repr::SchemaRows: a row of its widths");
        const auto pos = size();
        if (m_widths.bits() != 0)
        {
            const auto segment = Layout::segment_index(pos);
            auto* data = repr::detail::load_pointer<ThreadSafe, word>(m_segments[segment]);
            if (!data)
            {
                data = new word[segment_words(segment)]();
                repr::detail::store_pointer<ThreadSafe, word>(m_segments[segment], data);
            }
            write(data, Layout::segment_offset(pos, segment) * m_widths.bits(), row);
        }
        repr::detail::store_size<ThreadSafe>(m_size, pos + 1);
    }

    /// Moves the last row into `pos` and drops the last. Single-threaded.
    void swap_erase(size_type pos)
        requires(!ThreadSafe)
    {
        assert(pos < size());
        const auto last = size() - 1;
        if (pos != last && m_widths.bits() != 0)
        {
            const auto segment = Layout::segment_index(pos);
            write(m_segments[segment], Layout::segment_offset(pos, segment) * m_widths.bits(), (*this)[last]);
        }
        m_size = last;
    }

    void clear() noexcept { repr::detail::store_size<ThreadSafe>(m_size, 0); }

    /// Hashed and compared by values (SchemaRow::hash, equal).
    [[nodiscard]] static hash_t hash(const value_type& row) noexcept { return row.hash(); }
    [[nodiscard]] static bool equal(const value_type& lhs, const value_type& rhs) noexcept { return lhs.equal(rhs); }

private:
    /// Its rows' bits, and a word of room so that a read of the last column's bits never leaves it.
    [[nodiscard]] std::size_t segment_words(std::size_t segment) const noexcept
    { return bit::ceil_div<std::size_t>(Layout::segment_capacity(segment) * m_widths.bits(), 64) + 1; }
    template<bool OtherThreadSafe>
    void write(word* data, std::size_t first_bit, const SchemaRow<OtherThreadSafe>& row) const noexcept
    {
        for (std::size_t i = 0; i < m_widths.size(); ++i)
        {
            const auto& field = m_widths.at(i);
            words::set(data, first_bit + field.at, field.bits, row.get(i));
        }
    }

    std::array<repr::detail::Pointer<ThreadSafe, word>, Layout::max_segments> m_segments {};
    urd::column_widths m_widths;
    repr::detail::Size<ThreadSafe> m_size { 0 };
};

}  // namespace urd::repr

/// Its segments.
template<bool ThreadSafe>
struct urd::memory_of<urd::repr::SchemaRows<ThreadSafe>>
{
    static std::size_t bytes(const urd::repr::SchemaRows<ThreadSafe>& held)
    {
        auto total = std::size_t { 0 };
        for (std::size_t segment = 0; segment < held.m_segments.size(); ++segment)
            if (urd::repr::detail::load_pointer<ThreadSafe, std::uint64_t>(held.m_segments[segment]))
                total += held.segment_words(segment) * sizeof(std::uint64_t);
        return total;
    }
};

template<bool ThreadSafe>
inline constexpr bool std::ranges::enable_borrowed_range<urd::repr::SchemaRow<ThreadSafe>> = true;

/// A row and its columns read as T are values: hashed, compared and ordered by their elements.
template<bool ThreadSafe>
inline constexpr bool urd::value_range<urd::repr::SchemaRow<ThreadSafe>> = true;
template<bool ThreadSafe, typename T>
inline constexpr bool urd::value_range<std::ranges::subrange<urd::repr::detail::column_iterator<ThreadSafe, T>,
                                                             urd::repr::detail::column_iterator<ThreadSafe, T>,
                                                             std::ranges::subrange_kind::sized>> = true;
