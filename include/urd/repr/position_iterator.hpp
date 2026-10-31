#pragma once

// An iterator over a queue by position, for the queues that keep their elements in an array.

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

/// The elements of a queue from position 0 to size() - 1, each read through the queue's at_position (a ring's slot, a
/// heap's handle): for queues whose elements are not one range already.
template<typename Q>
class PositionIterator
{
public:
    using value_type = typename Q::value_type;
    using difference_type = std::ptrdiff_t;

    PositionIterator() = default;
    PositionIterator(const Q* queue, std::size_t position) : m_queue(queue), m_position(position) {}

    [[nodiscard]] const value_type& operator*() const { return m_queue->at_position(m_position); }
    PositionIterator& operator++()
    {
        ++m_position;
        return *this;
    }
    PositionIterator operator++(int)
    {
        auto before = *this;
        ++m_position;
        return before;
    }
    friend bool operator==(const PositionIterator&, const PositionIterator&) = default;

private:
    const Q* m_queue = nullptr;
    std::size_t m_position = 0;
};

}  // namespace urd::repr
