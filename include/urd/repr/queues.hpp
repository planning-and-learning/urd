#pragma once

#include "urd/elements.hpp"
#include "urd/semantics/less.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <utility>
#include <vector>

// Queues: elements reached by the order they leave in, with the standard adaptors' names (push, pop, front or top),
// and clear and reserve, which the adaptors lack, so that a queue reused per search keeps its storage.

namespace urd::repr
{

/// First in, first out, over a std::deque.
template<typename T>
class Fifo
{
public:
    using value_type = T;
    using size_type = std::size_t;

    [[nodiscard]] size_type size() const { return m_elements.size(); }
    [[nodiscard]] bool empty() const { return m_elements.empty(); }
    [[nodiscard]] const T& front() const { return m_elements.front(); }
    void push(const T& element) { m_elements.push_back(element); }
    void push(T&& element) { m_elements.push_back(std::move(element)); }
    template<typename... Args>
    void emplace(Args&&... args)
    { m_elements.emplace_back(std::forward<Args>(args)...); }
    void pop() { m_elements.pop_front(); }
    void clear() { m_elements.clear(); }

private:
    std::deque<T> m_elements;
};

/// Last in, first out, over a std::vector.
template<typename T>
class Lifo
{
public:
    using value_type = T;
    using size_type = std::size_t;

    [[nodiscard]] size_type size() const { return m_elements.size(); }
    [[nodiscard]] bool empty() const { return m_elements.empty(); }
    [[nodiscard]] const T& top() const { return m_elements.back(); }
    void push(const T& element) { m_elements.push_back(element); }
    void push(T&& element) { m_elements.push_back(std::move(element)); }
    template<typename... Args>
    void emplace(Args&&... args)
    { m_elements.emplace_back(std::forward<Args>(args)...); }
    void pop() { m_elements.pop_back(); }
    void clear() { m_elements.clear(); }
    void reserve(size_type count) { m_elements.reserve(count); }

private:
    std::vector<T> m_elements;
};

/// The least first by urd::Less: a binary heap in a std::vector.
template<typename T>
class Heap
{
public:
    using value_type = T;
    using size_type = std::size_t;

    [[nodiscard]] size_type size() const { return m_elements.size(); }
    [[nodiscard]] bool empty() const { return m_elements.empty(); }
    [[nodiscard]] const T& top() const { return m_elements.front(); }
    void push(const T& element)
    {
        m_elements.push_back(element);
        std::ranges::push_heap(m_elements, greater);
    }
    void push(T&& element)
    {
        m_elements.push_back(std::move(element));
        std::ranges::push_heap(m_elements, greater);
    }
    template<typename... Args>
    void emplace(Args&&... args)
    { push(T(std::forward<Args>(args)...)); }
    void pop()
    {
        std::ranges::pop_heap(m_elements, greater);
        m_elements.pop_back();
    }
    void clear() { m_elements.clear(); }
    void reserve(size_type count) { m_elements.reserve(count); }

private:
    /// The heap algorithms keep the greatest first; reversed, the least.
    static constexpr auto greater = [](const T& lhs, const T& rhs) { return Less<T> {}(rhs, lhs); };

    std::vector<T> m_elements;
};

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

template<orderable T>
struct container_traits<repr::Heap<T>> : properties_of<by_priority, shrinkable, retains>
{
};

}  // namespace urd
