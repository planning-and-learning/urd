#pragma once

// Queues, declared with their defaults and what the registry reads of them: first in, last in, the least by a
// comparator (a binary heap, an addressable heap, a bucket queue), and a ring for first-in queues that retain their
// storage. Defined in urd/repr/queues.hpp, which urd/use/queues.hpp includes.

#include "urd/elements.hpp"
#include "urd/semantics/less.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace urd::repr
{

/// Which element leaves first.
enum class leaving : std::uint8_t
{
    first_in,
    last_in,
    least,
};

/// A queue whose elements leave in the order Order says: first in (a deque), last in (a vector), the least by
/// Compare (a binary heap).
template<typename T, leaving Order, typename Compare = Less<T>>
class Queue;
/// The least first, each element reachable by the handle push gave (an indexed binary heap).
template<typename T, typename Compare = Less<T>>
class AddressableHeap;
/// The least first, for integral priorities: a vector of elements per priority.
template<typename T, typename Compare = Less<T>>
class BucketQueue;
/// A first-in queue over one vector used as a ring: pop and clear keep the vector.
template<typename T>
class RingFifo;

/// The integral priority a bucket queue files an element under: its projection where Compare orders by one, else the
/// element. A type computation, so that asking it of an element completes nothing.
template<typename T, typename Compare>
struct bucket_priority_of
{
    using type = T;
};
template<typename T, typename Compare>
    requires requires { Compare::projection; }
struct bucket_priority_of<T, Compare>
{
    using type = std::remove_cvref_t<std::invoke_result_t<decltype(Compare::projection), const T&>>;
};
template<typename T, typename Compare>
using bucket_priority_t = typename bucket_priority_of<T, Compare>::type;

template<typename T>
using Fifo = Queue<T, leaving::first_in>;
template<typename T>
using Lifo = Queue<T, leaving::last_in>;
template<typename T>
using Heap = Queue<T, leaving::least>;

}  // namespace urd::repr

namespace urd
{

/// A deque: a push or pop at either end moves no other element, so the front stays put while elements queue
/// behind it. Not retains: the deque frees its blocks on clear and as pops leave them.
template<typename T>
struct container_traits<repr::Fifo<T>> : properties_of<fifo, stable_refs, shrinkable>
{
};

template<typename T>
struct container_traits<repr::Lifo<T>> : properties_of<lifo, shrinkable, retains>
{
};

/// A ring over one vector: pop and clear keep it.
template<typename T>
struct container_traits<repr::RingFifo<T>> : properties_of<fifo, shrinkable, retains>
{
};

template<typename T, typename C>
    requires natural_order<C, T>
struct container_traits<repr::Queue<T, repr::leaving::least, C>> : properties_of<by_priority, shrinkable, retains>
{
};

template<typename T, typename C>
    requires natural_order<C, T>
struct container_traits<repr::AddressableHeap<T, C>> : properties_of<by_priority, handles, shrinkable, retains>
{
};

template<typename T, typename C>
    requires std::integral<repr::bucket_priority_t<T, C>> && natural_order<C, T>
struct container_traits<repr::BucketQueue<T, C>> : properties_of<by_priority, shrinkable, retains>
{
};
/// It is fast only where pushes never go below the last popped, so it serves only requests that say so.
template<typename T, typename C>
inline constexpr detail::property_set relies_on<repr::BucketQueue<T, C>> = detail::property_set::of({ tag::monotone_priority });

template<typename T, repr::leaving Order, typename C>
inline constexpr auto defined_in<repr::Queue<T, Order, C>> = detail::fixed_string("urd/use/queues.hpp");
template<typename T, typename C>
inline constexpr auto defined_in<repr::AddressableHeap<T, C>> = detail::fixed_string("urd/use/queues.hpp");
template<typename T, typename C>
inline constexpr auto defined_in<repr::BucketQueue<T, C>> = detail::fixed_string("urd/use/queues.hpp");
template<typename T>
inline constexpr auto defined_in<repr::RingFifo<T>> = detail::fixed_string("urd/use/queues.hpp");

}  // namespace urd
