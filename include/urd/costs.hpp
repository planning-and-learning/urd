#pragma once

// What each operation costs a representation, and the memory it takes, declared beside its traits: a specialization of
// container_costs<R> in the header that declares R's container_traits (urd/repr/*_fwd.hpp and the like), which names R
// without defining it. Reading a representation's costs therefore completes nothing and includes no implementation:
// the registry's own headers suffice. The docs' complexity tables, the benchmarks' judgment of the registry
// (benchmarks/record.py) and the counting tests (tests/complexity_verify.cpp) all read these.

#include "urd/traits.hpp"

#include <cstdint>
#include <string_view>

namespace urd
{

/// How a cost grows, in which quantity.
enum class growth : std::uint8_t
{
    one,
    log,
    linear,
    n_log_n,
    n_times,  ///< n times the quantity: O(n changes)
};

/// What a cost grows in.
enum class quantity : std::uint8_t
{
    n,         ///< the elements
    keys,      ///< U, the key range (dense keys fill it: about n)
    capacity,  ///< the slots held (about n over the load factor)
    row,       ///< r, a row's length
    delta,     ///< k, a delta's changes
    changes,   ///< the changes since a scope began, or since the log was cleared
};

/// One operation's cost: its growth in a quantity, and whether it is amortized or expected.
struct cost
{
    growth grows = growth::one;
    quantity of = quantity::n;
    bool amortized = false;
    bool expected = false;
    /// Verified from measured times (benchmarks/record.py) where no element operation shows the work: index and bit
    /// containers, clearing and iterating. Otherwise verified by counting the element operations
    /// (tests/complexity_verify.cpp).
    bool timed = false;
};

/// The costs of the operations of representation R, each a `static constexpr cost` member named for the operation
/// (insert, lookup, ordered_lookup, erase, clear, iterate, access, push, pop, top, merge, insert_row, find_row,
/// scope; an operation R lacks has none), and its memory: `words`, machine words per element (the element counting 1;
/// per key or slot for dense keys). What that memory holds is in words in its synopsis (urd/synopsis.hpp). Declared
/// for every registered representation; none here. A wrapper derives from its part's (part_costs) and declares what it changes, naming
/// its part as `part`.
template<typename R>
struct container_costs
{
};

namespace detail
{

template<typename List>
struct front_of
{
};
template<typename P, typename... Ps>
struct front_of<type_list<P, Ps...>>
{
    using type = P;
};

}  // namespace detail

/// The part a wrapper passes operations on to: the first of urd::parts<R>.
template<typename R>
using part_of = typename detail::front_of<typename parts<R>::type>::type;

/// The costs a wrapper starts from: its part's, with the part named.
template<typename Part>
struct part_costs : container_costs<Part>
{
    using part = Part;
};

/// The costs, named: growth, then a for amortized, e for expected, t for timed.
namespace costs
{
inline constexpr cost one {};
inline constexpr cost one_a { .amortized = true };
inline constexpr cost one_e { .expected = true };
inline constexpr cost one_ae { .amortized = true, .expected = true };
inline constexpr cost one_t { .timed = true };
inline constexpr cost one_at { .amortized = true, .timed = true };
inline constexpr cost log_n { .grows = growth::log };
inline constexpr cost log_na { .grows = growth::log, .amortized = true };
inline constexpr cost lin_n { .grows = growth::linear };
inline constexpr cost lin_nt { .grows = growth::linear, .timed = true };
inline constexpr cost lin_keys { .grows = growth::linear, .of = quantity::keys, .timed = true };
inline constexpr cost lin_capacity { .grows = growth::linear, .of = quantity::capacity, .timed = true };
inline constexpr cost lin_row_e { .grows = growth::linear, .of = quantity::row, .expected = true };
inline constexpr cost lin_row_t { .grows = growth::linear, .of = quantity::row, .timed = true };
inline constexpr cost lin_changes { .grows = growth::linear, .of = quantity::changes };
inline constexpr cost lin_changes_t { .grows = growth::linear, .of = quantity::changes, .timed = true };
inline constexpr cost nlog_changes { .grows = growth::n_log_n, .of = quantity::changes };
inline constexpr cost n_changes_t { .grows = growth::n_times, .of = quantity::changes, .timed = true };
}  // namespace costs

}  // namespace urd
