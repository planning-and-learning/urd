#pragma once

// What each operation costs a representation, and the memory it takes, declared beside its traits: a specialization of
// container_costs<R> in the header that declares R's container_traits (urd/repr/*_fwd.hpp and the like), which names R
// without defining it. Reading a representation's costs therefore completes nothing and includes no implementation:
// the registry's own headers suffice. The docs' complexity tables and the benchmarks' judgment of the registry
// (benchmarks/record.py) read these.

#include "urd/traits.hpp"

#include <cstdint>
#include <optional>
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
};

/// An operation's words (urd/property_set.hpp, words), and the operation Op (a struct with its name, its description,
/// and how costs name it). A kind added outside urd adds its own, with costs its containers declare as members named
/// for it.
using operation_info = words<struct operation_words>;
template<typename Op>
inline constexpr const operation_info* operation_v = &words_of<struct operation_words, Op>;

/// A list of operations: what a facet offers.
template<typename... Ops>
struct operation_list
{
};

/// urd's operations, each with the member of a container's costs named for it (in) and whether a wrapper's costs pass
/// it on from its part's (passed_on).
namespace operation
{

// One struct per operation: its words, the member of a container's costs named for it (in), and whether a wrapper's
// costs pass it on from its part's (passed_on: the wrapper inherits the part's member, the same object). A member's
// name cannot be a template argument, so the structs are written by this macro, local to this header.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define URD_OPERATION(member, words)                           \
    struct member                                              \
    {                                                          \
        static constexpr std::string_view name = #member;      \
        static constexpr std::string_view description = words; \
        template<typename C>                                   \
        static constexpr std::optional<cost> in()              \
        {                                                      \
            if constexpr (requires { C::member; })             \
                return C::member;                              \
            else                                               \
                return std::nullopt;                           \
        }                                                      \
        template<typename C, typename P>                       \
        static constexpr bool passed_on()                      \
        {                                                      \
            if constexpr (requires {                           \
                              C::member;                       \
                              P::member;                       \
                          })                                   \
                return &C::member == &P::member;               \
            else                                               \
                return false;                                  \
        }                                                      \
    }
// NOLINTEND(cppcoreguidelines-macro-usage)

URD_OPERATION(insert, "a set's or map's insert, a sequence's push_back");
URD_OPERATION(lookup, "find, contains, a map's value by key");
URD_OPERATION(ordered_lookup, "lower_bound");
URD_OPERATION(erase, "by key, or by position");
URD_OPERATION(clear, "every element at once");
URD_OPERATION(iterate, "every element in turn");
URD_OPERATION(access, "by position or handle");
URD_OPERATION(push, "a queue's");
URD_OPERATION(pop, "a queue's");
URD_OPERATION(top, "a queue's");
URD_OPERATION(merge, "a partition's");
URD_OPERATION(insert_row, "a pool's insert");
URD_OPERATION(find_row, "a pool's find");
URD_OPERATION(scope, "push_scope, changes, pop_scope");

#undef URD_OPERATION

}  // namespace operation

/// The cost of operation Op in costs C (a declaration's operation_costs, or urd::container_costs<R>): its member named
/// for the operation, or none where C has none.
template<typename Op, typename C>
constexpr std::optional<cost> cost_of()
{ return Op::template in<C>(); }

/// The costs of the operations of representation R, each a `static constexpr cost` member named for the operation
/// (insert, lookup, ordered_lookup, erase, clear, iterate, access, push, pop, top, merge, insert_row, find_row,
/// scope; an operation R lacks has none), and its memory: `words`, machine words per element (the element counting 1;
/// per key or slot for dense keys). What that memory holds is in words in its synopsis (urd/container_synopsis.hpp). Declared
/// for every registered representation, derived from its declaration (urd/registry/derived.hpp); none here. A wrapper's derive
/// from its part's and declare what they change, naming the part's declaration as `part`.
template<typename R>
struct container_costs
{
};

/// The costs, named: growth, then a for amortized, e for expected.
namespace costs
{
inline constexpr cost one {};
inline constexpr cost one_a { .amortized = true };
inline constexpr cost one_e { .expected = true };
inline constexpr cost one_ae { .amortized = true, .expected = true };
inline constexpr cost log_n { .grows = growth::log };
inline constexpr cost log_na { .grows = growth::log, .amortized = true };
inline constexpr cost lin_n { .grows = growth::linear };
inline constexpr cost lin_keys { .grows = growth::linear, .of = quantity::keys };
inline constexpr cost lin_capacity { .grows = growth::linear, .of = quantity::capacity };
inline constexpr cost lin_row { .grows = growth::linear, .of = quantity::row };
inline constexpr cost lin_row_e { .grows = growth::linear, .of = quantity::row, .expected = true };
inline constexpr cost lin_changes { .grows = growth::linear, .of = quantity::changes };
inline constexpr cost nlog_changes { .grows = growth::n_log_n, .of = quantity::changes };
}  // namespace costs

}  // namespace urd
