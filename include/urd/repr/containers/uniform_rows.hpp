#pragma once

// Rows stored one after another in segments that never move, each laid out as a codec says: the element storage of a
// set of rows (urd/repr/values/indexed.hpp, Rows).

#include "urd/memory.hpp"
#include "urd/repr/containers/byte_arena.hpp"
#include "urd/repr/containers/detail/geometric_segment_layout.hpp"
#include "urd/repr/containers/detail/threading.hpp"
#include "urd/repr/containers/segment_sizes.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/core/position_iterator.hpp"

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <ranges>
#include <span>
#include <type_traits>
#include <variant>

namespace urd::repr
{

/// Rows appended and read back by position, which stay where they lie while rows are appended. The codec says how a
/// row is stored: in units (Codec::unit, trivially copyable), units(rows, length) of them for rows of `length`
/// elements; write(data, at, row) stores a row as the at-th of those at `data`, read(data, at, length) hands it out
/// (Codec::value_type), and check(row) throws where a row cannot be stored. It is built with a Codec::parameter
/// (std::monostate: nothing).
///
/// While every row has the length of the first, row i is the i-th of segments of geometrically growing size (counted
/// in rows): no offsets or lengths are stored. From the first row of another length on, a table holds each row as it
/// is handed out, and new rows are written to a byte arena; nothing moves. With ThreadSafe, rows below size() may be
/// read while one thread (under the owner's lock) appends. Clear needs no other operation running; it keeps the
/// segments for rows of the same length.
template<typename Codec, bool ThreadSafe>
class UniformRows
{
    friend struct urd::memory_of<UniformRows>;

    using unit = typename Codec::unit;
    static_assert(std::is_trivially_copyable_v<unit>, "urd::repr::UniformRows: rows stored in trivially copyable units");
    using Layout = detail::GeometricSegmentLayout<default_first_array_segment_size>;

public:
    /// What it is built with.
    using parameter = typename Codec::parameter;
    using value_type = typename Codec::value_type;
    using reference = value_type;  ///< a row is handed out by value
    using const_reference = value_type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using const_iterator = PositionIterator<const UniformRows>;
    using iterator = const_iterator;

    UniformRows()
        requires std::default_initializable<Codec>
    = default;
    explicit UniformRows(const parameter& built_with)
        requires std::constructible_from<Codec, const parameter&>
        : m_codec(built_with)
    {
    }
    UniformRows(const UniformRows&) = delete;
    UniformRows& operator=(const UniformRows&) = delete;
    ~UniformRows()
    {
        for (auto& segment : m_segments)
            delete[] detail::load(segment);
    }

    [[nodiscard]] size_type size() const noexcept { return detail::load(m_size); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    [[nodiscard]] const_iterator begin() const { return const_iterator(this, 0); }
    [[nodiscard]] const_iterator end() const { return const_iterator(this, size()); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend() const { return end(); }

    /// The table holds a row once rows of other lengths came; until then (and while it takes the earlier rows over)
    /// the row lies in its segment.
    // ponytail: a row of length 0 has no segment; the codec reads it from no data.
    [[nodiscard]] value_type operator[](size_type pos) const
    {
        if (pos < m_table.size())
            return m_table[pos];
        const auto segment = Layout::segment_index(pos);
        return m_codec.read(detail::load(m_segments[segment]), Layout::segment_offset(pos, segment), m_length);
    }

    /// Throws where `row` cannot be stored (Codec::check): what an owner calls before it touches anything, so that a
    /// row that does not fit changes nothing.
    template<typename Row>
    void check(const Row& row) const
    { m_codec.check(row); }

    /// Appends a row, copied: by one writer at a time (the owner's lock). A row that cannot be stored throws before
    /// anything is written.
    template<typename Row>
    void push_back(const Row& row)
    {
        m_codec.check(row);
        const auto pos = size();
        const auto length = static_cast<size_type>(std::ranges::size(row));
        // The length of the rows to come, from the first. Segments kept by clear (sized for their rows) keep theirs.
        if (pos == 0 && !detail::load(m_segments[0]))
            m_length = length;
        if (!m_table.empty() || length != m_length)
        {
            // The first row of another length: every row so far into the table, where readers find them from then on
            // (the segments stay, for readers still there).
            for (auto earlier = m_table.size(); earlier < pos; ++earlier)
                m_table.push_back((*this)[earlier]);
            const auto bytes = m_codec.units(1, length) * sizeof(unit);
            auto* data = static_cast<unit*>(std::memset(m_arena.allocate(bytes, alignof(unit)), 0, bytes));
            m_codec.write(data, 0, row);
            m_table.push_back(m_codec.read(data, 0, length));
        }
        else if (length != 0)
        {
            const auto segment = Layout::segment_index(pos);
            auto* data = detail::load(m_segments[segment]);
            if (!data)
            {
                // ponytail: zeroed for every codec (packed words are read before they are written; plain rows pay a pass)
                data = new unit[m_codec.units(Layout::segment_capacity(segment), length)]();
                detail::store(m_segments[segment], data);
            }
            m_codec.write(data, Layout::segment_offset(pos, segment), row);
        }
        detail::store(m_size, pos + 1);
    }

    /// Moves the last row into `pos` and drops the last: the row at `pos` changes, the others stay. Where rows have
    /// other lengths, the arena keeps the erased row's bytes until clear. Single-threaded.
    void swap_erase(size_type pos)
        requires(!ThreadSafe)
    {
        assert(pos < size());
        const auto last = size() - 1;
        if (!m_table.empty())
        {
            m_table[pos] = m_table[last];
            m_table.pop_back();
        }
        else if (pos != last && m_length != 0)
        {
            const auto segment = Layout::segment_index(pos);
            m_codec.write(m_segments[segment], Layout::segment_offset(pos, segment), (*this)[last]);
        }
        m_size = last;
    }

    void clear() noexcept
    {
        detail::store(m_size, 0);
        m_table.clear();
        m_arena.clear();
    }

private:
    std::array<detail::Pointer<ThreadSafe, unit>, Layout::max_segments> m_segments {};
    [[no_unique_address]] Codec m_codec;
    size_type m_length = 0;
    detail::Size<ThreadSafe> m_size { 0 };
    SegmentedVector<value_type, default_first_segment_size, ThreadSafe> m_table;
    ByteArena m_arena;
};

/// Rows of trivially copyable elements, stored as they are and handed out as spans.
template<typename U>
struct PlainCodec
{
    using unit = U;
    using value_type = std::span<const U>;
    using parameter = std::monostate;

    [[nodiscard]] static std::size_t units(std::size_t rows, std::size_t length) noexcept { return rows * length; }
    [[nodiscard]] static value_type read(const U* data, std::size_t at, std::size_t length) noexcept { return value_type(data + (at * length), length); }
    static void write(U* data, std::size_t at, value_type row) noexcept
    {
        if (!row.empty())  // an empty span may have no data, which memcpy does not take
            std::memcpy(data + (at * row.size()), row.data(), row.size_bytes());
    }
    static void check(value_type /*row*/) noexcept {}
};

}  // namespace urd::repr

/// Its segments, and where rows of other lengths came, the table and the arena.
template<typename Codec, bool ThreadSafe>
struct urd::memory_of<urd::repr::UniformRows<Codec, ThreadSafe>>
{
    static std::size_t bytes(const urd::repr::UniformRows<Codec, ThreadSafe>& held)
    {
        using Rows = urd::repr::UniformRows<Codec, ThreadSafe>;
        auto total = urd::memory_usage(held.m_table) + urd::memory_usage(held.m_arena);
        for (std::size_t segment = 0; segment < held.m_segments.size(); ++segment)
            if (urd::repr::detail::load(held.m_segments[segment]))
                total += held.m_codec.units(Rows::Layout::segment_capacity(segment), held.m_length) * sizeof(typename Rows::unit);
        return total;
    }
};
