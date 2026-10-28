#pragma once

#include "urd/elements.hpp"
#include "urd/semantics/less.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <type_traits>
#include <utility>
#include <vector>

// Queues: elements reached by the order they leave in, with the standard adaptors' names (push, pop, front or top),
// and clear and reserve, which the adaptors lack, so that a queue reused per search keeps its storage.

namespace urd::repr
{

/// Which element leaves first.
enum class leaving : std::uint8_t
{
    first_in,
    last_in,
    least,
};

/// A queue whose elements leave in the order Order says: first in, over a std::deque; last in, over a std::vector;
/// the least by Compare (urd::Less, or the order a request gave), as a binary heap in a std::vector.
template<typename T, leaving Order, typename Compare = Less<T>>
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
    { return m_elements.front(); }
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

template<typename T>
using Fifo = Queue<T, leaving::first_in>;
template<typename T>
using Lifo = Queue<T, leaving::last_in>;
template<typename T>
using Heap = Queue<T, leaving::least>;

}  // namespace urd::repr

namespace urd
{

template<typename T>
struct container_traits<repr::Fifo<T>> : properties_of<fifo, shrinkable>
{
};

template<typename T>
struct container_traits<repr::Lifo<T>> : properties_of<lifo, shrinkable, retains>
{
};

template<typename T, typename C>
    requires natural_order<C, T>
struct container_traits<repr::Queue<T, repr::leaving::least, C>> : properties_of<by_priority, shrinkable, retains>
{
};

}  // namespace urd
