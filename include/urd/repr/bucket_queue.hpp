#pragma once

// The least first, for integral priorities: a vector of elements per priority.

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

namespace urd::repr
{

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

}  // namespace urd::repr
