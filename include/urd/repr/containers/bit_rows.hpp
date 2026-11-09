#pragma once

// Rows of index-like elements stored in a few bits each: how the element storage of a pool of rows whose request
// promises urd::bit lays them out (urd/repr/pools/pools.hpp).

#include "urd/elements/requirements.hpp"
#include "urd/repr/containers/bit.hpp"
#include "urd/repr/containers/uniform_rows.hpp"
#include "urd/tags.hpp"

#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace urd::repr
{

namespace detail
{

/// The packed values of rows (urd/repr/containers/bit.hpp): read and written at an absolute bit, through relaxed atomics
/// where threads read rows while one appends (a new row's first bits may share a block with a row being read).
template<bool ThreadSafe>
struct packed_words
{
    using word = std::uint64_t;

    /// The `width` bits at bit `at` (width 1..64), which may span two words.
    [[nodiscard]] static word get(const word* words, std::size_t at, std::uint8_t width) noexcept
    {
        const auto from = std::span<const word>(words + (at / 64), 2);
        const auto offset = static_cast<std::uint8_t>(at % 64);
        if constexpr (ThreadSafe)
            return bit::atomic_read_int<word>(from, offset, width);
        else
            return bit::read_int<word>(from, offset, width);
    }
    /// Writes `value` into the `width` bits at bit `at`, keeping the bits around them.
    static void set(word* words, std::size_t at, std::uint8_t width, word value) noexcept
    {
        const auto into = std::span<word>(words + (at / 64), 2);
        const auto offset = static_cast<std::uint8_t>(at % 64);
        if constexpr (ThreadSafe)
            bit::atomic_write_int<word>(into, value, offset, width);
        else
            bit::write_int<word>(into, value, offset, width);
    }
};

}  // namespace detail

/// A row of a BitCodec: its elements decoded on access, as values; a random-access borrowed view (its iterators hold
/// where it lies). Valid while the rows are.
template<typename U, bool ThreadSafe>
class PackedRow : public std::ranges::view_interface<PackedRow<U, ThreadSafe>>
{
    using word = std::uint64_t;
    using words = detail::packed_words<ThreadSafe>;

public:
    using value_type = U;
    using size_type = std::size_t;

    class const_iterator
    {
    public:
        using value_type = U;
        using reference = U;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept = std::random_access_iterator_tag;

        const_iterator() = default;
        const_iterator(const PackedRow& row, size_type pos) : m_words(row.m_words), m_first_bit(row.m_first_bit), m_width(row.m_width), m_pos(pos) {}

        reference operator*() const
        { return index_of<U>::from_index(static_cast<std::size_t>(words::get(m_words, m_first_bit + (m_pos * m_width), m_width))); }
        reference operator[](difference_type n) const { return *(*this + n); }
        const_iterator& operator++() { return *this += 1; }
        const_iterator operator++(int)
        {
            auto copy = *this;
            ++*this;
            return copy;
        }
        const_iterator& operator--() { return *this -= 1; }
        const_iterator operator--(int)
        {
            auto copy = *this;
            --*this;
            return copy;
        }
        const_iterator& operator+=(difference_type n)
        {
            m_pos = static_cast<size_type>(static_cast<difference_type>(m_pos) + n);
            return *this;
        }
        const_iterator& operator-=(difference_type n) { return *this += -n; }
        friend const_iterator operator+(const_iterator it, difference_type n) { return it += n; }
        friend const_iterator operator+(difference_type n, const_iterator it) { return it += n; }
        friend const_iterator operator-(const_iterator it, difference_type n) { return it -= n; }
        friend difference_type operator-(const const_iterator& lhs, const const_iterator& rhs)
        { return static_cast<difference_type>(lhs.m_pos) - static_cast<difference_type>(rhs.m_pos); }
        friend bool operator==(const const_iterator& lhs, const const_iterator& rhs) { return lhs.m_pos == rhs.m_pos; }
        friend auto operator<=>(const const_iterator& lhs, const const_iterator& rhs) { return lhs.m_pos <=> rhs.m_pos; }

    private:
        const word* m_words = nullptr;
        std::size_t m_first_bit = 0;
        std::uint8_t m_width = 1;
        size_type m_pos = 0;
    };
    using iterator = const_iterator;

    PackedRow() = default;
    PackedRow(const word* data, std::size_t first_bit, size_type length, std::uint8_t width) :
        m_words(data),
        m_first_bit(first_bit),
        m_length(length),
        m_width(width)
    {
    }

    [[nodiscard]] U operator[](size_type pos) const
    {
        assert(pos < m_length);
        return index_of<U>::from_index(static_cast<std::size_t>(words::get(m_words, m_first_bit + (pos * m_width), m_width)));
    }
    [[nodiscard]] size_type size() const noexcept { return m_length; }
    [[nodiscard]] bool empty() const noexcept { return m_length == 0; }
    [[nodiscard]] const_iterator begin() const { return const_iterator(*this, 0); }
    [[nodiscard]] const_iterator end() const { return const_iterator(*this, m_length); }

private:
    const word* m_words = nullptr;
    std::size_t m_first_bit = 0;
    size_type m_length = 0;
    std::uint8_t m_width = 1;
};

/// Rows of index-like elements (index_of: integers, enums, urd::index), each element in the bit width the codec is
/// built with, back to back; handed out as rows that decode on access (PackedRow). What a UniformRows of them is laid
/// out by (urd/repr/containers/uniform_rows.hpp).
template<typename U, bool ThreadSafe>
struct BitCodec
{
    static_assert(indexable<U>, "urd::repr::BitCodec: elements that are indices (urd::index_of)");

    using unit = std::uint64_t;
    using value_type = PackedRow<U, ThreadSafe>;
    using parameter = urd::bit_width;

    explicit BitCodec(urd::bit_width built_with) : width(built_with.bits)
    { assert(width >= 1 && width <= 64 && "urd::repr::BitCodec: a bit width of 1 to 64"); }

    /// The words of the rows' bits, and a word of room so that a read of the last element's bits never leaves them.
    [[nodiscard]] std::size_t units(std::size_t rows, std::size_t length) const noexcept
    { return bit::ceil_div<std::size_t>(rows * length * width, 64) + 1; }
    [[nodiscard]] value_type read(const unit* data, std::size_t at, std::size_t length) const noexcept
    { return value_type(data, at * length * width, length, width); }
    template<std::ranges::sized_range Row>
    void write(unit* data, std::size_t at, const Row& row) const
    {
        auto at_bit = at * std::ranges::size(row) * width;
        for (const auto& element : row)
        {
            detail::packed_words<ThreadSafe>::set(data, at_bit, width, static_cast<unit>(index_of<std::remove_cvref_t<decltype(element)>>::to_index(element)));
            at_bit += width;
        }
    }
    /// Throws std::out_of_range where an element of `row` does not fit in the width (the caller's promise, urd::bit,
    /// broken).
    template<std::ranges::input_range Row>
    void check(const Row& row) const
    {
        for (const auto& element : row)
            if ((static_cast<unit>(index_of<std::remove_cvref_t<decltype(element)>>::to_index(element)) & ~bit::lo_set<unit>[width]) != 0)
                throw std::out_of_range("urd::bit: an element does not fit in the bit width the Store was built with");
    }

    std::uint8_t width;
};

}  // namespace urd::repr

template<typename U, bool ThreadSafe>
inline constexpr bool std::ranges::enable_borrowed_range<urd::repr::PackedRow<U, ThreadSafe>> = true;
