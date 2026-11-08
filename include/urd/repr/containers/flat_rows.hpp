#pragma once

// Rows of plain elements stored flat, in segments that never move: the element storage of a set of rows
// (urd/repr/values/indexed.hpp, Rows).

#include "urd/memory.hpp"
#include "urd/repr/core/position_iterator.hpp"
#include "urd/repr/containers/byte_arena.hpp"
#include "urd/repr/containers/detail/geometric_segment_layout.hpp"
#include "urd/repr/containers/detail/threading.hpp"
#include "urd/repr/containers/segment_sizes.hpp"
#include "urd/repr/containers/segmented_vector.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>

namespace urd::repr
{

/// Rows of trivially copyable elements, appended and read as spans, which stay valid while rows are appended. While
/// every row has the width of the first, row i lies at i * width in segments of geometrically growing size (counted
/// in rows): no offsets or lengths are stored. The first row of another width switches to a table of spans (one per
/// row), the rows from then on written to a byte arena; nothing moves. With ThreadSafe, rows below size() may be read
/// while one thread (under the owner's lock) appends. Clear needs no other operation running; it keeps the segments
/// for rows of the same width.
template<typename U, bool ThreadSafe>
class FlatRows
{
    static_assert(std::is_trivially_copyable_v<U>, "urd::repr::FlatRows: rows of trivially copyable elements");
    friend struct urd::memory_of<FlatRows>;

    using Layout = detail::GeometricSegmentLayout<default_first_array_segment_size>;
    using Spans = SegmentedVector<std::span<const U>, default_first_segment_size, ThreadSafe>;
    using Flag = std::conditional_t<ThreadSafe, std::atomic<bool>, bool>;

public:
    using value_type = std::span<const U>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using const_iterator = PositionIterator<const FlatRows>;
    using iterator = const_iterator;

    FlatRows() = default;
    FlatRows(const FlatRows&) = delete;
    FlatRows& operator=(const FlatRows&) = delete;
    FlatRows(FlatRows&&) = delete;
    FlatRows& operator=(FlatRows&&) = delete;
    ~FlatRows() { release(); }

    [[nodiscard]] size_type size() const noexcept { return detail::load_size<ThreadSafe>(m_size); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    [[nodiscard]] const_iterator begin() const { return const_iterator(this, 0); }
    [[nodiscard]] const_iterator end() const { return const_iterator(this, size()); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend() const { return end(); }

    [[nodiscard]] value_type operator[](size_type pos) const
    {
        if (mixed())
            return m_spans[pos];
        const auto segment = Layout::segment_index(pos);
        const auto* data = detail::load_pointer<ThreadSafe, U>(m_segments[segment]);
        // ponytail: a row of width 0 has no storage; its span is empty.
        return data ? value_type(data + (Layout::segment_offset(pos, segment) * m_width), m_width) : value_type();
    }

    /// Appends a row, copied: by one writer at a time (the owner's lock).
    void push_back(std::span<const U> row)
    {
        const auto pos = size();
        if (pos == 0 && !mixed())
            start(row.size());
        if (!mixed() && row.size() != m_width)
            switch_to_spans(pos);
        if (mixed())
        {
            const auto* bytes = m_arena.write(reinterpret_cast<const std::uint8_t*>(row.data()), row.size_bytes(), alignof(U));
            m_spans.push_back(value_type(reinterpret_cast<const U*>(bytes), row.size()));
        }
        else if (m_width != 0)
        {
            const auto segment = Layout::segment_index(pos);
            auto* data = detail::load_pointer<ThreadSafe, U>(m_segments[segment]);
            if (!data)
            {
                data = std::allocator<U>().allocate(Layout::segment_capacity(segment) * m_width);
                detail::store_pointer<ThreadSafe, U>(m_segments[segment], data);
            }
            std::memcpy(data + (Layout::segment_offset(pos, segment) * m_width), row.data(), row.size_bytes());
        }
        detail::store_size<ThreadSafe>(m_size, pos + 1);
    }

    /// Moves the last row into `pos` and drops the last: the row at `pos` (its span) changes, the others stay. Where
    /// rows have other widths, the arena keeps the erased row's bytes until clear. Single-threaded.
    void swap_erase(size_type pos)
        requires(!ThreadSafe)
    {
        assert(pos < size());
        const auto last = size() - 1;
        if (mixed())
        {
            if (pos != last)
                m_spans[pos] = m_spans[last];
            m_spans.pop_back();
        }
        else if (pos != last && m_width != 0)
        {
            const auto at = [this](size_type row)
            {
                const auto segment = Layout::segment_index(row);
                return m_segments[segment] + (Layout::segment_offset(row, segment) * m_width);
            };
            std::memcpy(at(pos), at(last), m_width * sizeof(U));
        }
        m_size = last;
    }

    void clear() noexcept
    {
        detail::store_size<ThreadSafe>(m_size, 0);
        set_mixed(false);
        m_spans.clear();
        m_arena.clear();
    }

private:
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
    /// The width of the rows to come, from the first. Segments kept from before (sized for their width: the storage is
    /// retained) keep their width, and rows of another one go to the table of spans.
    void start(size_type width)
    {
        if (!detail::load_pointer<ThreadSafe, U>(m_segments[0]))
            m_width = width;
    }
    /// Every row so far into the table of spans, then the table is used: readers of earlier rows may still read
    /// them in the segments, which stay.
    void switch_to_spans(size_type count)
    {
        for (size_type pos = 0; pos < count; ++pos)
            m_spans.push_back((*this)[pos]);
        set_mixed(true);
    }
    void release() noexcept
    {
        for (size_type segment = 0; segment < m_segments.size(); ++segment)
            if (auto* data = detail::load_pointer<ThreadSafe, U>(m_segments[segment]))
            {
                std::allocator<U>().deallocate(data, Layout::segment_capacity(segment) * m_width);
                detail::store_pointer<ThreadSafe, U>(m_segments[segment], static_cast<U*>(nullptr));
            }
    }

    std::array<detail::Pointer<ThreadSafe, U>, Layout::max_segments> m_segments {};
    size_type m_width = 0;
    detail::Size<ThreadSafe> m_size { 0 };
    Flag m_mixed { false };
    Spans m_spans;
    ByteArena m_arena;
};

}  // namespace urd::repr

/// Its segments, and where rows of other widths came, the table of spans and the arena.
template<typename U, bool ThreadSafe>
struct urd::memory_of<urd::repr::FlatRows<U, ThreadSafe>>
{
    static std::size_t bytes(const urd::repr::FlatRows<U, ThreadSafe>& held)
    {
        using Layout = typename urd::repr::FlatRows<U, ThreadSafe>::Layout;
        auto total = urd::memory_usage(held.m_spans) + urd::memory_usage(held.m_arena);
        for (std::size_t segment = 0; segment < held.m_segments.size(); ++segment)
            if (urd::repr::detail::load_pointer<ThreadSafe, U>(held.m_segments[segment]))
                total += Layout::segment_capacity(segment) * held.m_width * sizeof(U);
        return total;
    }
};
