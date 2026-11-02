#pragma once

// A queue whose elements leave in the order it says: first in, last in, or the least by a comparator.

#include "urd/elements/less.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/core/std_memory.hpp"
#include "urd/repr/queues/position_iterator.hpp"
#include "urd/repr/queues/queues_fwd.hpp"
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

/// A queue whose elements leave in the order Order says: first in, over a std::deque; last in, over a std::vector;
/// the least by Compare (urd::Less, or the order a request gave), as a binary heap in a std::vector.
template<typename T, leaving Order, typename Compare>
class Queue
{
    friend struct urd::memory_of<Queue>;

    static constexpr bool fifo = Order == leaving::first_in;
    using Elements = std::conditional_t<fifo, std::deque<T>, std::vector<T>>;

public:
    using value_type = T;
    using size_type = std::size_t;

    [[nodiscard]] size_type size() const { return m_elements.size(); }
    [[nodiscard]] bool empty() const { return m_elements.empty(); }
    [[nodiscard]] const T& front() const
        requires fifo
    { return m_elements.front(); }
    [[nodiscard]] const T& back() const
        requires fifo
    { return m_elements.back(); }
    /// The elements in the order pushed (first in, last in), or in no particular order (the least first).
    [[nodiscard]] auto begin() const { return m_elements.cbegin(); }
    [[nodiscard]] auto end() const { return m_elements.cend(); }
    [[nodiscard]] const T& top() const
        requires(!fifo)
    { return Order == leaving::last_in ? m_elements.back() : m_elements.front(); }

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
    { m_elements.reserve(count); }

private:
    /// The heap algorithms keep the greatest first; reversed, the least.
    static constexpr auto greater = [](const auto& lhs, const auto& rhs) { return Compare {}(rhs, lhs); };

    Elements m_elements;
};

}  // namespace urd::repr

namespace urd
{

template<>
struct include<"urd/repr/queues/queue.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<fifo_decl, S>
{
    using type = repr::Fifo<typename S::element_type>;
};
template<typename S>
struct representation_of<lifo_decl, S>
{
    using type = repr::Lifo<typename S::element_type>;
};
template<typename S>
struct representation_of<heap_decl, S>
{
    using type = repr::Queue<typename S::element_type, repr::leaving::least, typename S::order_type>;
};

}  // namespace urd::detail

/// Its elements: a deque for first in, first out, else a vector.
template<typename T, urd::repr::leaving Order, typename Compare>
struct urd::memory_of<urd::repr::Queue<T, Order, Compare>>
{
    static std::size_t bytes(const repr::Queue<T, Order, Compare>& held) { return memory_usage(held.m_elements); }
};
