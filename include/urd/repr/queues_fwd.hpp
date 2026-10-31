#pragma once

// Queues, declared with their defaults and what the registry reads of them: first in, last in, the least by a
// comparator (a binary heap, an addressable heap, a bucket queue), and a ring for first-in queues that retain their
// storage. Defined in urd/repr/queue.hpp, addressable_heap.hpp, bucket_queue.hpp and ring_fifo.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/element_concepts.hpp"
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
inline constexpr auto defined_in<repr::Queue<T, Order, C>> = detail::fixed_string("urd/repr/queue.hpp");
template<typename T, typename C>
inline constexpr auto defined_in<repr::AddressableHeap<T, C>> = detail::fixed_string("urd/repr/addressable_heap.hpp");
template<typename T, typename C>
inline constexpr auto defined_in<repr::BucketQueue<T, C>> = detail::fixed_string("urd/repr/bucket_queue.hpp");
template<typename T>
inline constexpr auto defined_in<repr::RingFifo<T>> = detail::fixed_string("urd/repr/ring_fifo.hpp");

}  // namespace urd

namespace urd
{

/// fifo.
template<typename T>
struct container_costs<repr::Fifo<T>>
{
    static constexpr cost push = costs::one_a;
    static constexpr cost pop = costs::one;
    static constexpr cost top = costs::one_t;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 1.2;
};

/// ring_fifo.
template<typename T>
struct container_costs<repr::RingFifo<T>>
{
    static constexpr cost push = costs::one_a;
    static constexpr cost pop = costs::one;
    static constexpr cost top = costs::one_t;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 1.5;
};

/// lifo.
template<typename T>
struct container_costs<repr::Lifo<T>>
{
    static constexpr cost push = costs::one_a;
    static constexpr cost pop = costs::one;
    static constexpr cost top = costs::one_t;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 1.5;
};

/// heap, by urd::Less or by an order_by.
template<typename T, typename Compare>
struct container_costs<repr::Queue<T, repr::leaving::least, Compare>>
{
    static constexpr cost push = costs::log_na;
    static constexpr cost pop = costs::log_n;
    static constexpr cost top = costs::one_t;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 1.5;
};

/// addressable_heap.
template<typename T, typename Compare>
struct container_costs<repr::AddressableHeap<T, Compare>>
{
    static constexpr cost push = costs::log_na;
    static constexpr cost pop = costs::log_n;
    static constexpr cost top = costs::one_t;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 3;
};

/// bucket_queue.
template<typename T, typename Compare>
struct container_costs<repr::BucketQueue<T, Compare>>
{
    static constexpr cost push = costs::one_at;
    static constexpr cost pop = costs::one_at;
    static constexpr cost top = costs::one_t;
    static constexpr cost clear = costs::lin_keys;
    static constexpr double words = 2;
};

}  // namespace urd

namespace urd
{

template<typename T>
struct container_synopsis<repr::Fifo<T>>
{
    static constexpr std::string_view id = "fifo";
    static constexpr std::string_view description = "queue (a deque, first in first out)";
    static constexpr std::string_view holds = "a deque; stable references";
};

template<typename T>
struct container_synopsis<repr::RingFifo<T>>
{
    static constexpr std::string_view id = "ring_fifo";
    static constexpr std::string_view description = "ring fifo (one vector used as a ring, kept on pop and clear)";
    static constexpr std::string_view holds = "one vector as a ring; retains";
};

template<typename T>
struct container_synopsis<repr::Lifo<T>>
{
    static constexpr std::string_view id = "lifo";
    static constexpr std::string_view description = "stack (a vector, last in first out)";
    static constexpr std::string_view holds = "a vector; retains";
};

template<typename T, typename Compare>
struct container_synopsis<repr::Queue<T, repr::leaving::least, Compare>>
{
    static constexpr std::string_view id = "heap";
    static constexpr std::string_view description = "binary heap (a vector, least first, by urd::Less or an order_by)";
    static constexpr std::string_view holds = "a vector as a binary heap; retains";
};

template<typename T, typename Compare>
struct container_synopsis<repr::Queue<T, repr::leaving::first_in, Compare>>
{
    static constexpr std::string_view id = "fifo_by";
    static constexpr std::string_view description = "queue by a given order";
    static constexpr std::string_view holds = "a deque; stable references";
};

template<typename T, typename Compare>
struct container_synopsis<repr::Queue<T, repr::leaving::last_in, Compare>>
{
    static constexpr std::string_view id = "lifo_by";
    static constexpr std::string_view description = "queue by a given order";
    static constexpr std::string_view holds = "a vector; retains";
};

template<typename T, typename Compare>
struct container_synopsis<repr::AddressableHeap<T, Compare>>
{
    static constexpr std::string_view id = "addressable_heap";
    static constexpr std::string_view description = "addressable heap (push gives a handle to update or erase by)";
    static constexpr std::string_view holds = "a heap of handles + a slot per handle";
};

template<typename T, typename Compare>
struct container_synopsis<repr::BucketQueue<T, Compare>>
{
    static constexpr std::string_view id = "bucket_queue";
    static constexpr std::string_view description = "bucket queue (a bucket per integral priority)";
    static constexpr std::string_view holds = "a vector per priority";
};

}  // namespace urd
