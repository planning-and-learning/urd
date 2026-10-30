#pragma once

#include "urd/elements.hpp"
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

// Queues: elements reached by the order they leave in, with the standard adaptors' names (push, pop, front or top),
// and clear and reserve, which the adaptors lack, so that a queue reused run after run keeps its storage.

namespace urd::repr
{

/// A queue whose elements leave in the order Order says: first in, over a std::deque; last in, over a std::vector;
/// the least by Compare (urd::Less, or the order a request gave), as a binary heap in a std::vector.
template<typename T, leaving Order, typename Compare>
class Queue
{
    static constexpr bool fifo = Order == leaving::first_in;
    using Elements = std::conditional_t<fifo, std::deque<T>, std::vector<T>>;

public:
    using value_type = T;
    using size_type = std::size_t;

    [[nodiscard]] size_type size() const { return m_elements.size(); }
    [[nodiscard]] bool empty() const { return m_elements.empty(); }
    [[nodiscard]] const T& front() const
        requires fifo
    {
        return m_elements.front();
    }
    [[nodiscard]] const T& top() const
        requires(!fifo)
    {
        return Order == leaving::last_in ? m_elements.back() : m_elements.front();
    }

    void push(const T& element) { emplace(element); }
    void push(T&& element) { emplace(std::move(element)); }
    template<typename... Args>
    void emplace(Args&&... args)
    {
        m_elements.emplace_back(std::forward<Args>(args)...);
        if constexpr (Order == leaving::least)
            std::ranges::push_heap(m_elements, greater);
    }
    void pop()
    {
        if constexpr (fifo)
            m_elements.pop_front();
        else
        {
            if constexpr (Order == leaving::least)
                std::ranges::pop_heap(m_elements, greater);
            m_elements.pop_back();
        }
    }
    void clear() { m_elements.clear(); }
    void reserve(size_type count)
        requires(!fifo)
    {
        m_elements.reserve(count);
    }

private:
    /// The heap algorithms keep the greatest first; reversed, the least.
    static constexpr auto greater = [](const auto& lhs, const auto& rhs) { return Compare {}(rhs, lhs); };

    Elements m_elements;
};

/// The least first by Compare, each element reachable by the handle push gave: update moves it to its new place, erase
/// removes it wherever it is (an indexed binary heap). The handles of removed elements are given out again.
template<typename T, typename Compare>
class AddressableHeap
{
public:
    using value_type = T;
    using size_type = std::size_t;
    using handle_type = std::size_t;

    [[nodiscard]] size_type size() const { return m_heap.size(); }
    [[nodiscard]] bool empty() const { return m_heap.empty(); }
    [[nodiscard]] const T& top() const { return m_slots[m_heap.front()].element; }

    handle_type push(const T& element) { return emplace(element); }
    handle_type push(T&& element) { return emplace(std::move(element)); }
    template<typename... Args>
    handle_type emplace(Args&&... args)
    {
        auto handle = m_slots.size();
        if (m_free.empty())
            m_slots.push_back(slot { T(std::forward<Args>(args)...), m_heap.size() });
        else
        {
            handle = m_free.back();
            m_free.pop_back();
            m_slots[handle] = slot { T(std::forward<Args>(args)...), m_heap.size() };
        }
        m_heap.push_back(handle);
        sift_up(m_heap.size() - 1);
        return handle;
    }
    void pop() { erase(m_heap.front()); }
    void update(handle_type handle, const T& element)
    {
        m_slots[handle].element = element;
        sift_up(m_slots[handle].position);
        sift_down(m_slots[handle].position);
    }
    void erase(handle_type handle)
    {
        const auto at = m_slots[handle].position;
        const auto last = m_heap.back();
        m_heap.pop_back();
        if (at < m_heap.size())
        {
            place(at, last);
            sift_up(at);
            sift_down(m_slots[last].position);
        }
        m_free.push_back(handle);
    }
    void clear()
    {
        m_heap.clear();
        m_slots.clear();
        m_free.clear();
    }
    void reserve(size_type count)
    {
        m_heap.reserve(count);
        m_slots.reserve(count);
    }

private:
    struct slot
    {
        T element;
        size_type position;
    };

    [[nodiscard]] bool before(handle_type lhs, handle_type rhs) const { return Compare {}(m_slots[lhs].element, m_slots[rhs].element); }
    void place(size_type at, handle_type handle)
    {
        m_heap[at] = handle;
        m_slots[handle].position = at;
    }
    void sift_up(size_type at)
    {
        const auto handle = m_heap[at];
        while (at > 0 && before(handle, m_heap[(at - 1) / 2]))
        {
            place(at, m_heap[(at - 1) / 2]);
            at = (at - 1) / 2;
        }
        place(at, handle);
    }
    void sift_down(size_type at)
    {
        const auto handle = m_heap[at];
        for (auto child = 2 * at + 1; child < m_heap.size(); child = 2 * at + 1)
        {
            if (child + 1 < m_heap.size() && before(m_heap[child + 1], m_heap[child]))
                ++child;
            if (!before(m_heap[child], handle))
                break;
            place(at, m_heap[child]);
            at = child;
        }
        place(at, handle);
    }

    std::vector<handle_type> m_heap;  // handles, as a binary heap by their elements
    std::vector<slot> m_slots;        // by handle: the element and where its handle is in the heap
    std::vector<handle_type> m_free;  // handles to give out again
};

template<typename T, typename Compare>
[[nodiscard]] constexpr bucket_priority_t<T, Compare> bucket_priority(const T& element)
{
    if constexpr (requires { Compare::projection; })
        return std::invoke(Compare::projection, element);
    else
        return element;
}

/// The least first, for integral priorities: a vector of elements per priority, and the least that holds any. A push
/// below it lowers it, so the order holds whatever is pushed; where pushes never go below the last popped
/// (monotone_priority), the scan for the next least only moves forward: push O(1), pop amortized O(1) over the range
/// of priorities. Equal priorities leave last in first. Clear keeps every bucket's storage.
template<typename T, typename Compare>
class BucketQueue
{
public:
    using value_type = T;
    using size_type = std::size_t;

    [[nodiscard]] size_type size() const { return m_size; }
    [[nodiscard]] bool empty() const { return m_size == 0; }
    [[nodiscard]] const T& top() const { return m_buckets[m_least].back(); }

    void push(const T& element) { emplace(element); }
    void push(T&& element) { emplace(std::move(element)); }
    template<typename... Args>
    void emplace(Args&&... args)
    {
        auto element = T(std::forward<Args>(args)...);
        const auto priority = index_of(element);
        if (priority >= m_buckets.size())
            m_buckets.resize(priority + 1);
        m_buckets[priority].push_back(std::move(element));
        if (m_size == 0 || priority < m_least)
            m_least = priority;
        ++m_size;
    }
    void pop()
    {
        m_buckets[m_least].pop_back();
        if (--m_size > 0)
            while (m_buckets[m_least].empty())
                ++m_least;
    }
    void clear()
    {
        for (auto& bucket : m_buckets)
            bucket.clear();
        m_size = 0;
        m_least = 0;
    }

private:
    [[nodiscard]] static size_type index_of(const T& element)
    {
        const auto priority = bucket_priority<T, Compare>(element);
        assert(priority >= 0 && "urd::repr::BucketQueue: priorities are indices, never negative");
        return static_cast<size_type>(priority);
    }

    std::vector<std::vector<T>> m_buckets;
    size_type m_size = 0;
    size_type m_least = 0;
};

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
        return *m_ring[m_head];
    }

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

}  // namespace urd
