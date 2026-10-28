#pragma once

#include "urd/store/types.hpp"

#include <utility>

namespace urd::detail::facet
{

/// A queue: push, pop, clear, and the next element to leave, by the standard adaptors' names: front() where the first in leaves
/// first (std::queue), top() otherwise (std::stack, std::priority_queue).
template<typename Derived, typename R, typename S>
class queue_access
{
    using value_type = typename store_types<R, S>::value_type;

public:
    void push(const value_type& element) { representation().push(element); }
    void push(value_type&& element) { representation().push(std::move(element)); }
    template<typename... Args>
    void emplace(Args&&... args)
    { representation().emplace(std::forward<Args>(args)...); }
    /// Removes the next element; the Store must not be empty.
    void pop() { representation().pop(); }
    /// Removes every element; a monotone queue never empties but by pop.
    void clear()
        requires has<S, shrinkable>
    { representation().clear(); }

    [[nodiscard]] const value_type& front() const
        requires has<S, fifo>
    { return representation().front(); }
    [[nodiscard]] const value_type& top() const
        requires(!has<S, fifo>)
    { return representation().top(); }

private:
    [[nodiscard]] const R& representation() const { return store_access::representation(static_cast<const Derived&>(*this)); }
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet
