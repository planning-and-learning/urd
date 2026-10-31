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

/// The operations a cost is declared for, the same for every kind of Store; a representation declares a cost per
/// operation it has, as a member named for it (container_costs).
enum class operation : std::uint8_t
{
    insert,          ///< a set's or map's insert, a sequence's push_back
    lookup,          ///< find, contains, a map's value by key
    ordered_lookup,  ///< lower_bound
    erase,           ///< by key, or by position
    clear,
    iterate,
    access,  ///< by position or handle
    push,    ///< a queue's
    pop,
    top,
    merge,       ///< a partition's
    insert_row,  ///< a pool's insert
    find_row,    ///< a pool's find
    scope,       ///< push_scope, changes, pop_scope
    count        ///< not an operation: how many there are
};

/// An operation's name, as its member in container_costs.
constexpr std::string_view name(operation of)
{
    switch (of)
    {
        case operation::insert:
            return "insert";
        case operation::lookup:
            return "lookup";
        case operation::ordered_lookup:
            return "ordered_lookup";
        case operation::erase:
            return "erase";
        case operation::clear:
            return "clear";
        case operation::iterate:
            return "iterate";
        case operation::access:
            return "access";
        case operation::push:
            return "push";
        case operation::pop:
            return "pop";
        case operation::top:
            return "top";
        case operation::merge:
            return "merge";
        case operation::insert_row:
            return "insert_row";
        case operation::find_row:
            return "find_row";
        case operation::scope:
            return "scope";
        case operation::count:
            break;
    }
    return {};
}

/// How a cost grows, in which quantity.
enum class growth : std::uint8_t
{
    one,
    log,
    linear,
    n_log_n,
    n_times,  ///< n times the quantity: O(n changes)
};

/// A growth's name: one, log, linear, n_log_n, n_times.
constexpr std::string_view name(growth of)
{
    switch (of)
    {
        case growth::one:
            return "one";
        case growth::log:
            return "log";
        case growth::linear:
            return "linear";
        case growth::n_log_n:
            return "n_log_n";
        case growth::n_times:
            return "n_times";
    }
    return {};
}

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

/// A quantity's name: n, keys, capacity, row, delta, changes.
constexpr std::string_view name(quantity of)
{
    switch (of)
    {
        case quantity::n:
            return "n";
        case quantity::keys:
            return "keys";
        case quantity::capacity:
            return "capacity";
        case quantity::row:
            return "row";
        case quantity::delta:
            return "delta";
        case quantity::changes:
            return "changes";
    }
    return {};
}

/// A quantity as a cost writes it: n, U, capacity, r, k, changes.
constexpr std::string_view symbol(quantity of)
{
    switch (of)
    {
        case quantity::n:
            return "n";
        case quantity::keys:
            return "U";
        case quantity::capacity:
            return "capacity";
        case quantity::row:
            return "r";
        case quantity::delta:
            return "k";
        case quantity::changes:
            return "changes";
    }
    return {};
}

/// Which quantities grow together: the elements, their key range and the capacity are one; a row's length, a delta's
/// changes and the changes since a scope began each grow on their own.
enum class scale : std::uint8_t
{
    n,
    row,
    delta,
    changes,
};
/// A scale's name.
constexpr std::string_view name(scale of)
{
    switch (of)
    {
        case scale::n:
            return "n";
        case scale::row:
            return "row";
        case scale::delta:
            return "delta";
        case scale::changes:
            return "changes";
    }
    return {};
}
constexpr scale scale_of(quantity of)
{
    switch (of)
    {
        case quantity::n:
        case quantity::keys:
        case quantity::capacity:
            return scale::n;
        case quantity::row:
            return scale::row;
        case quantity::delta:
            return scale::delta;
        case quantity::changes:
            return scale::changes;
    }
    return scale::n;
}

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
/// per key or slot for dense keys). What that memory holds is in words in its synopsis (urd/container_synopsis.hpp). Declared
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
