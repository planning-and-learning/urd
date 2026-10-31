#pragma once

// A first-in queue over one vector used as a ring.

#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/position_iterator.hpp"
#include "urd/repr/queues_fwd.hpp"
#include "urd/semantics/less.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A first-in queue over one vector used as a ring: the elements lie from `head` on, wrapping (the size a power of
/// two, so a slot is a mask away); a push past the end unrolls the ring into a vector of twice the size. Pop and
/// clear keep the vector, so a queue emptied and refilled allocates nothing: retains, where a deque frees its blocks.
template<typename T>
class RingFifo
{
public:
    using value_type = T;
    using size_type = std::size_t;

    [[nodiscard]] size_type size() const { return m_count; }
    [[nodiscard]] bool empty() const { return m_count == 0; }
    [[nodiscard]] const T& front() const
    {
        assert(!empty() && "urd::repr::RingFifo::front: the queue is empty");
        return at_position(0);
    }
    [[nodiscard]] const T& back() const
    {
        assert(!empty() && "urd::repr::RingFifo::back: the queue is empty");
        return at_position(m_count - 1);
    }
    /// The elements in the order pushed.
    [[nodiscard]] PositionIterator<RingFifo> begin() const { return { this, 0 }; }
    [[nodiscard]] PositionIterator<RingFifo> end() const { return { this, m_count }; }

    void push(const T& element) { emplace(element); }
    void push(T&& element) { emplace(std::move(element)); }
    template<typename... Args>
    void emplace(Args&&... args)
    {
        if (m_count == m_ring.size())
            grow();
        m_ring[slot(m_count)].emplace(std::forward<Args>(args)...);
        ++m_count;
    }
    void pop()
    {
        assert(!empty() && "urd::repr::RingFifo::pop: the queue is empty");
        m_ring[m_head].reset();
        m_head = slot(1);
        --m_count;
    }
    /// Destroys the elements; the ring stays.
    void clear()
    {
        for (; m_count > 0; --m_count)
        {
            m_ring[m_head].reset();
            m_head = slot(1);
        }
        m_head = 0;
    }
    void reserve(size_type count)
    {
        if (count > m_ring.size())
            unroll(std::bit_ceil(count));
    }

private:
    friend class PositionIterator<RingFifo>;
    /// The element `position` places after the head.
    [[nodiscard]] const T& at_position(size_type position) const
    {
        assert(position < m_count);
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access): the m_count slots from the head on each hold an element
        return *m_ring[slot(position)];
    }
    /// The slot `offset` elements after the head.
    [[nodiscard]] size_type slot(size_type offset) const { return (m_head + offset) & (m_ring.size() - 1); }
    void grow() { unroll(std::max<size_type>(4, 2 * m_ring.size())); }
    /// A ring of `capacity` slots (a power of two) holding the elements from slot 0 on.
    void unroll(size_type capacity)
    {
        auto ring = std::vector<std::optional<T>>(capacity);
        for (size_type each = 0; each < m_count; ++each)
            ring[each] = std::move(m_ring[slot(each)]);
        m_ring = std::move(ring);
        m_head = 0;
    }

    std::vector<std::optional<T>> m_ring;
    size_type m_head = 0;
    size_type m_count = 0;
};

}  // namespace urd::repr

namespace urd
{

template<>
struct include<"urd/repr/ring_fifo.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<ring_fifo_decl, S>
{
    using type = repr::RingFifo<typename S::element_type>;
};

}  // namespace urd::detail
