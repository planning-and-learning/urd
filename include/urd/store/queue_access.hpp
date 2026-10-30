#pragma once

#include "urd/store/types.hpp"

#include <utility>

namespace urd::detail::facet
{

/// A queue: push, pop, clear, and the next element to leave, by the standard adaptors' names: front() where the first in leaves
/// first (std::queue), top() otherwise (std::stack, std::priority_queue).
/// What reaches an element of a queue with handles: the representation's handle_type; void for other queues.
template<typename R, bool Handles>
struct queue_handle
{
    using type = void;
};
template<typename R>
struct queue_handle<R, true>
{
    using type = typename R::handle_type;
};

template<typename Derived, typename R, typename S>
class queue_access
{
    using value_type = typename store_types<R, S>::value_type;

public:
    using handle_type = typename queue_handle<R, has<S, handles>>::type;

    /// With handles, the handle that reaches the element later (update, erase).
    decltype(auto) push(const value_type& element) { return representation().push(element); }
    decltype(auto) push(value_type&& element) { return representation().push(std::move(element)); }
    template<typename... Args>
    decltype(auto) emplace(Args&&... args)
    { return representation().emplace(std::forward<Args>(args)...); }
    /// Removes the next element; the Store must not be empty.
    void pop() { representation().pop(); }
    /// Removes every element; a monotone queue never empties but by pop.
    void clear()
        requires has<S, shrinkable>
    { representation().clear(); }

    /// Replaces the element of `handle`, which moves to its place in the order.
    template<typename Handle = handle_type>
        requires has<S, handles>
    void update(const Handle& handle, const value_type& element)
    { representation().update(handle, element); }
    /// Removes the element of `handle`, wherever it is in the order.
    template<typename Handle = handle_type>
        requires has<S, handles>
    void erase(const Handle& handle)
    { representation().erase(handle); }

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
