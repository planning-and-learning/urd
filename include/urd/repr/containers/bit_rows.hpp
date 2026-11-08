#pragma once

// Rows of index-like elements stored in a few bits each, in segments that never move: the element storage of a pool of
// rows whose request promises urd::bit (urd/repr/pools/pools.hpp).

#include "urd/elements/requirements.hpp"
#include "urd/repr/core/position_iterator.hpp"
#include "urd/repr/containers/bit.hpp"
#include "urd/memory.hpp"
#include "urd/repr/containers/detail/geometric_segment_layout.hpp"
#include "urd/repr/containers/detail/threading.hpp"
#include "urd/repr/containers/segment_sizes.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/tags.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

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

/// A row of a BitRows: its elements decoded on access, as values; a random-access borrowed view (its iterators hold
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

/// Rows of index-like elements (index_of: integers, enums, urd::index), each element in the bit width given at
/// construction. While every row has the length of the first, row i lies at bit i * length * width of segments of
/// geometrically growing size (in rows), which never move: no offsets are stored. The first row of another length
/// switches to a table of rows (where each lies, how long), the rows from then on written to segments of their own.
/// With ThreadSafe, rows below size() may be read while one thread (under the owner's lock) appends. Clear keeps the
/// segments.
template<typename U, bool ThreadSafe>
class BitRows
{
    static_assert(indexable<U>, "urd::repr::BitRows: elements that are indices (urd::index_of)");
    friend struct urd::memory_of<BitRows>;

    using word = std::uint64_t;
    using words = detail::packed_words<ThreadSafe>;
    using Layout = detail::GeometricSegmentLayout<default_first_array_segment_size>;
    using Flag = std::conditional_t<ThreadSafe, std::atomic<bool>, bool>;
    /// A row where rows have other lengths: its words, its first bit there, its length.
    struct Placed
    {
        const word* data;
        std::size_t first_bit;
        std::size_t length;
    };
    using Table = SegmentedVector<Placed, default_first_segment_size, ThreadSafe>;
    /// Words of rows of other lengths, in segments that never move; a row never spans two.
    struct Spill
    {
        std::unique_ptr<word[]> data;
        std::size_t words;
    };

public:
    using value_type = PackedRow<U, ThreadSafe>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using const_iterator = PositionIterator<const BitRows>;
    using iterator = const_iterator;

    explicit BitRows(urd::bit_width width) : m_width(width.bits)
    { assert(m_width >= 1 && m_width <= 64 && "urd::repr::BitRows: a bit width of 1 to 64"); }
    BitRows(const BitRows&) = delete;
    BitRows& operator=(const BitRows&) = delete;
    BitRows(BitRows&&) = delete;
    BitRows& operator=(BitRows&&) = delete;
    ~BitRows() { release(); }

    [[nodiscard]] urd::bit_width width() const noexcept { return urd::bit_width { m_width }; }
    [[nodiscard]] size_type size() const noexcept { return repr::detail::load_size<ThreadSafe>(m_size); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    [[nodiscard]] const_iterator begin() const { return const_iterator(this, 0); }
    [[nodiscard]] const_iterator end() const { return const_iterator(this, size()); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend() const { return end(); }

    [[nodiscard]] value_type operator[](size_type pos) const
    {
        if (mixed())
        {
            const auto& placed = m_table[pos];
            return value_type(placed.data, placed.first_bit, placed.length, m_width);
        }
        const auto segment = Layout::segment_index(pos);
        return value_type(repr::detail::load_pointer<ThreadSafe, word>(m_segments[segment]),
                          Layout::segment_offset(pos, segment) * row_bits(),
                          m_length,
                          m_width);
    }

    /// Appends a row of the given elements: by one writer at a time (the owner's lock). An element that does not fit
    /// in the width (the caller's promise, urd::bit, broken) throws std::out_of_range before anything is written.
    template<std::ranges::sized_range Row>
    void push_back(const Row& row)
    {
        check(width(), row);
        const auto pos = size();
        const auto length = static_cast<std::size_t>(std::ranges::size(row));
        if (pos == 0 && !mixed() && !repr::detail::load_pointer<ThreadSafe, word>(m_segments[0]))
            m_length = length;
        if (!mixed() && length != m_length)
            switch_to_table(pos);
        if (mixed())
        {
            auto [data, first_bit] = spill(length);
            write(data, first_bit, row);
            m_table.push_back(Placed { data, first_bit, length });
        }
        else if (row_bits() != 0)
        {
            const auto segment = Layout::segment_index(pos);
            auto* data = repr::detail::load_pointer<ThreadSafe, word>(m_segments[segment]);
            if (!data)
            {
                data = new word[segment_words(segment)]();
                repr::detail::store_pointer<ThreadSafe, word>(m_segments[segment], data);
            }
            write(data, Layout::segment_offset(pos, segment) * row_bits(), row);
        }
        repr::detail::store_size<ThreadSafe>(m_size, pos + 1);
    }

    /// Throws std::out_of_range where an element of `row` does not fit in `width` (the caller's promise, urd::bit,
    /// broken): what an owner calls before it touches anything, so that a row that does not fit changes nothing.
    template<std::ranges::input_range Row>
    static void check(urd::bit_width width, const Row& row)
    {
        for (const auto& element : row)
            if ((static_cast<word>(index_of<std::remove_cvref_t<decltype(element)>>::to_index(element)) & ~bit::lo_set<word>[width.bits]) != 0)
                throw std::out_of_range("urd::bit: an element does not fit in the bit width the Store was built with");
    }

    /// Moves the last row into `pos` and drops the last: the row at `pos` changes, the others stay. Single-threaded.
    void swap_erase(size_type pos)
        requires(!ThreadSafe)
    {
        assert(pos < size());
        const auto last = size() - 1;
        if (mixed())
        {
            if (pos != last)
                m_table[pos] = m_table[last];
            m_table.pop_back();
        }
        else if (pos != last && row_bits() != 0)
        {
            const auto moved = (*this)[last];
            const auto segment = Layout::segment_index(pos);
            write(m_segments[segment], Layout::segment_offset(pos, segment) * row_bits(), moved);
        }
        m_size = last;
    }

    void clear() noexcept
    {
        repr::detail::store_size<ThreadSafe>(m_size, 0);
        set_mixed(false);
        m_table.clear();
        m_spill_used = 0;
        m_spill_at = 0;
    }

private:
    [[nodiscard]] std::size_t row_bits() const noexcept { return m_length * m_width; }
    /// The words of a segment: its rows' bits, and a word of room so that a read of the last element's bits never
    /// leaves it.
    [[nodiscard]] std::size_t segment_words(std::size_t segment) const noexcept
    { return bit::ceil_div<std::size_t>(Layout::segment_capacity(segment) * row_bits(), 64) + 1; }


    template<typename Row>
    void write(word* data, std::size_t first_bit, const Row& row) const
    {
        auto at = first_bit;
        for (const auto& element : row)
        {
            words::set(data, at, m_width, static_cast<word>(index_of<std::remove_cvref_t<decltype(element)>>::to_index(element)));
            at += m_width;
        }
    }
    /// Room for a row of `length` elements among the rows of other lengths: where its bits start.
    std::pair<word*, std::size_t> spill(std::size_t length)
    {
        const auto needed = bit::ceil_div<std::size_t>(length * m_width, 64) + 1;
        while (m_spill_used < m_spill.size() && m_spill_at + needed > m_spill[m_spill_used].words)
        {
            ++m_spill_used;
            m_spill_at = 0;
        }
        if (m_spill_used == m_spill.size())
        {
            const auto words_in = std::max<std::size_t>(needed, m_spill.empty() ? 64 : m_spill.back().words * 2);
            m_spill.push_back(Spill { std::unique_ptr<word[]>(new word[words_in]()), words_in });
            m_spill_at = 0;
        }
        auto* data = m_spill[m_spill_used].data.get() + m_spill_at;
        m_spill_at += needed - 1;  // the last word may be the next row's first
        return { data, 0 };
    }
    [[nodiscard]] bool mixed() const noexcept
    {
        if constexpr (ThreadSafe)
            return m_mixed.load(std::memory_order_acquire);
        else
            return m_mixed;
    }
    void set_mixed(bool value) noexcept
    {
        if constexpr (ThreadSafe)
            m_mixed.store(value, std::memory_order_release);
        else
            m_mixed = value;
    }
    /// Every row so far into the table, then the table is used: readers of earlier rows may still read them in the
    /// segments, which stay.
    void switch_to_table(size_type count)
    {
        for (size_type pos = 0; pos < count; ++pos)
        {
            const auto segment = Layout::segment_index(pos);
            m_table.push_back(Placed { repr::detail::load_pointer<ThreadSafe, word>(m_segments[segment]),
                                       Layout::segment_offset(pos, segment) * row_bits(),
                                       m_length });
        }
        set_mixed(true);
    }
    void release() noexcept
    {
        for (auto& segment : m_segments)
            delete[] repr::detail::load_pointer<ThreadSafe, word>(segment);
    }

    std::array<repr::detail::Pointer<ThreadSafe, word>, Layout::max_segments> m_segments {};
    std::uint8_t m_width;
    std::size_t m_length = 0;
    repr::detail::Size<ThreadSafe> m_size { 0 };
    Flag m_mixed { false };
    Table m_table;
    std::vector<Spill> m_spill;
    std::size_t m_spill_used = 0;
    std::size_t m_spill_at = 0;
};

}  // namespace urd::repr

/// Its segments, and where rows of other lengths came, the table and the words they take.
template<typename U, bool ThreadSafe>
struct urd::memory_of<urd::repr::BitRows<U, ThreadSafe>>
{
    static std::size_t bytes(const urd::repr::BitRows<U, ThreadSafe>& held)
    {
        auto total = urd::memory_usage(held.m_table) + (held.m_spill.capacity() * sizeof(typename urd::repr::BitRows<U, ThreadSafe>::Spill));
        for (const auto& spill : held.m_spill)
            total += spill.words * sizeof(std::uint64_t);
        for (std::size_t segment = 0; segment < held.m_segments.size(); ++segment)
            if (urd::repr::detail::load_pointer<ThreadSafe, std::uint64_t>(held.m_segments[segment]))
                total += held.segment_words(segment) * sizeof(std::uint64_t);
        return total;
    }
};

template<typename U, bool ThreadSafe>
inline constexpr bool std::ranges::enable_borrowed_range<urd::repr::PackedRow<U, ThreadSafe>> = true;
