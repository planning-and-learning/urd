#pragma once

// A byte arena: segments that never move, so what was written stays where it is. Copied in shape from yggdrasil
// (include/yggdrasil/buffer/segmented_buffer.hpp), names moved to urd::repr.

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace urd::repr
{

/// Bytes written one block after another into segments that never move: a block's address stays valid until the
/// arena is cleared. Segments double in size; clear keeps them for reuse (retains).
class ByteArena
{
    struct Segment
    {
        std::unique_ptr<std::uint8_t[]> storage;
        std::size_t capacity;
        std::size_t size = 0;

        explicit Segment(std::size_t capacity_) : storage(std::make_unique_for_overwrite<std::uint8_t[]>(capacity_)), capacity(capacity_) {}
    };

public:
    static constexpr std::size_t default_first_segment_bytes = std::size_t { 1 } << 12U;

    explicit ByteArena(std::size_t first_segment_bytes = default_first_segment_bytes) :
        m_segment_bytes(std::bit_ceil(std::max<std::size_t>(first_segment_bytes, 1)))
    {
    }

    /// Copies `amount` bytes in, at an address aligned to `align` (a power of two), and gives that address.
    const std::uint8_t* write(const std::uint8_t* data, std::size_t amount, std::size_t align = 1)
    {
        assert(data != nullptr || amount == 0);
        assert(std::has_single_bit(align));
        const auto padding = align - 1;
        if (amount > std::numeric_limits<std::size_t>::max() - padding)
            throw std::length_error("urd::repr::ByteArena: a block too large to address");
        make_room(amount + padding);
        auto& segment = m_segments[m_current];
        const auto aligned = (m_position + padding) & ~padding;
        auto* at = segment.storage.get() + aligned;
        if (amount > 0)
            std::memcpy(at, data, amount);
        m_size += amount + (aligned - m_position);
        m_position = aligned + amount;
        segment.size = m_position;
        return at;
    }

    /// Everything written is forgotten; the segments stay for what comes next.
    void clear() noexcept
    {
        for (auto& segment : m_segments)
            segment.size = 0;
        m_current = 0;
        m_position = 0;
        m_size = 0;
    }

    /// The bytes written, padding included.
    [[nodiscard]] std::size_t size() const noexcept { return m_size; }
    [[nodiscard]] bool empty() const noexcept { return m_size == 0; }
    /// The bytes held by the segments.
    [[nodiscard]] std::size_t capacity() const noexcept { return m_capacity; }
    [[nodiscard]] std::size_t segments() const noexcept { return m_segments.size(); }

private:
    /// A current segment with `needed` bytes free: this one, a later one kept from before a clear, or a new one.
    void make_room(std::size_t needed)
    {
        if (m_current < m_segments.size() && needed <= m_segments[m_current].capacity - m_position)
            return;
        for (auto later = m_current + 1; later < m_segments.size(); ++later)
        {
            if (needed <= m_segments[later].capacity)
            {
                m_current = later;
                m_position = 0;
                return;
            }
        }
        constexpr auto largest = std::bit_floor(std::numeric_limits<std::size_t>::max() / 2);
        if (needed > largest || m_segment_bytes > largest)
            throw std::length_error("urd::repr::ByteArena: a block too large to address");
        m_segment_bytes = 2 * std::max(std::bit_ceil(needed), m_segment_bytes);
        m_segments.emplace_back(m_segment_bytes);
        m_capacity += m_segment_bytes;
        m_current = m_segments.size() - 1;
        m_position = 0;
    }

    std::size_t m_segment_bytes;
    std::vector<Segment> m_segments;
    std::size_t m_current = 0;
    std::size_t m_position = 0;
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
};

}  // namespace urd::repr
