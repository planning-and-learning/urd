#pragma once

// Queues, declared with their defaults and what the registry reads of them: first in, last in, the least by a
// comparator (a binary heap, an addressable heap, a bucket queue), and a ring for first-in queues that retain their
// storage. Defined in urd/repr/queues/queue.hpp, addressable_heap.hpp, bucket_queue.hpp and ring_fifo.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
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

namespace detail
{

/// queue (a deque, first in first out)
struct fifo_decl
{
    static constexpr std::string_view id = "fifo";
    static constexpr auto headers = header_list<1> { "urd/repr/queues/queue.hpp" };
    using kind = arrival_queue_kind;
    static constexpr property_set provides = property_set::of({ access::fifo, referencing::stable_refs });
    static constexpr role_tags relies_on {};
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost push = costs::one_a;
        static constexpr cost pop = costs::one;
        static constexpr cost top = costs::one;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr double words = 1.2;
    };
    static constexpr std::string_view description = "queue (a deque, first in first out)";
    static constexpr std::string_view holds = "a deque; stable references";
};
template<typename T>
struct declaration_of<repr::Fifo<T>>
{
    using declaration = fifo_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// ring fifo (one vector used as a ring, kept on pop and clear)
struct ring_fifo_decl
{
    static constexpr std::string_view id = "ring_fifo";
    static constexpr auto headers = header_list<1> { "urd/repr/queues/ring_fifo.hpp" };
    using kind = arrival_queue_kind;
    static constexpr property_set provides = property_set::of({ access::fifo, memory::retains });
    static constexpr role_tags relies_on {};
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost push = costs::one_a;
        static constexpr cost pop = costs::one;
        static constexpr cost top = costs::one;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "ring fifo (one vector used as a ring, kept on pop and clear)";
    static constexpr std::string_view holds = "one vector as a ring; retains";
};
template<typename T>
struct declaration_of<repr::RingFifo<T>>
{
    using declaration = ring_fifo_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// stack (a vector, last in first out)
struct lifo_decl
{
    static constexpr std::string_view id = "lifo";
    static constexpr auto headers = header_list<1> { "urd/repr/queues/queue.hpp" };
    using kind = arrival_queue_kind;
    static constexpr property_set provides = property_set::of({ access::lifo, memory::retains });
    static constexpr role_tags relies_on {};
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost push = costs::one_a;
        static constexpr cost pop = costs::one;
        static constexpr cost top = costs::one;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "stack (a vector, last in first out)";
    static constexpr std::string_view holds = "a vector; retains";
};
template<typename T>
struct declaration_of<repr::Lifo<T>>
{
    using declaration = lifo_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace detail

namespace detail
{

/// binary heap (a vector, least first, by urd::Less or an order_by)
struct heap_decl
{
    static constexpr std::string_view id = "heap";
    static constexpr auto headers = header_list<1> { "urd/repr/queues/queue.hpp" };
    using kind = priority_queue_kind;
    static constexpr property_set provides = property_set::of({ access::by_priority, memory::retains });
    static constexpr role_tags relies_on = relies(traits<role::order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost push = costs::log_na;
        static constexpr cost pop = costs::log_n;
        static constexpr cost top = costs::one;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "binary heap (a vector, least first, by urd::Less or an order_by)";
    static constexpr std::string_view holds = "a vector as a binary heap; retains";
};
template<typename T, typename Compare>
struct declaration_of<repr::Queue<T, repr::leaving::least, Compare>>
{
    using declaration = heap_decl;
    static constexpr role_tags roles = element_roles<T, Compare>();
};

}  // namespace detail

namespace detail
{

/// addressable heap (push gives a handle to update or erase by)
struct addressable_heap_decl
{
    static constexpr std::string_view id = "addressable_heap";
    static constexpr auto headers = header_list<1> { "urd/repr/queues/addressable_heap.hpp" };
    using kind = priority_queue_kind;
    static constexpr property_set provides = property_set::of({ access::by_priority, handling::handles, memory::retains });
    static constexpr role_tags relies_on = relies(traits<role::order, trait::natural_ordered>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost push = costs::log_na;
        static constexpr cost pop = costs::log_n;
        static constexpr cost top = costs::one;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr double words = 3;
    };
    static constexpr std::string_view description = "addressable heap (push gives a handle to update or erase by)";
    static constexpr std::string_view holds = "a heap of handles + a slot per handle";
};
template<typename T, typename Compare>
struct declaration_of<repr::AddressableHeap<T, Compare>>
{
    using declaration = addressable_heap_decl;
    static constexpr role_tags roles = element_roles<T, Compare>();
};

}  // namespace detail

namespace detail
{

/// bucket queue (a bucket per integral priority)
struct bucket_queue_decl
{
    static constexpr std::string_view id = "bucket_queue";
    static constexpr auto headers = header_list<1> { "urd/repr/queues/bucket_queue.hpp" };
    using kind = priority_queue_kind;
    static constexpr property_set provides = property_set::of({ access::by_priority, memory::retains });
    static constexpr role_tags relies_on =
        relies(stated({ priority::monotone_priority }), traits<role::order, trait::natural_ordered, trait::integral_priority>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost push = costs::one_a;
        static constexpr cost pop = costs::one_a;
        static constexpr cost top = costs::one;
        static constexpr cost clear = costs::lin_keys;
        static constexpr double words = 2;
    };
    static constexpr std::string_view description = "bucket queue (a bucket per integral priority)";
    static constexpr std::string_view holds = "a vector per priority";
};
template<typename T, typename Compare>
struct declaration_of<repr::BucketQueue<T, Compare>>
{
    using declaration = bucket_queue_decl;
    static constexpr role_tags roles = element_roles<T, Compare>();
};

}  // namespace detail

}  // namespace urd
