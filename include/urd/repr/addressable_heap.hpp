#pragma once

// The least first, each element reachable by the handle push gave: an indexed binary heap.

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
    /// The element of `handle`, which must be in the queue.
    [[nodiscard]] const T& operator[](handle_type handle) const { return m_slots[handle].element; }
    /// The elements in no particular order.
    [[nodiscard]] PositionIterator<AddressableHeap> begin() const { return { this, 0 }; }
    [[nodiscard]] PositionIterator<AddressableHeap> end() const { return { this, m_heap.size() }; }

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
    friend class PositionIterator<AddressableHeap>;
    struct slot
    {
        T element;
        size_type position;
    };

    [[nodiscard]] const T& at_position(size_type position) const { return m_slots[m_heap[position]].element; }

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

}  // namespace urd::repr

namespace urd
{

template<>
struct include<"urd/repr/addressable_heap.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<addressable_heap_decl, S>
{
    using type = repr::AddressableHeap<typename S::element_type, element_order_t<S>>;
};

}  // namespace urd::detail
