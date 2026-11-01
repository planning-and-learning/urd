#pragma once

// What each operation costs a representation, and the memory it takes: stated in its declaration (operation_costs,
// words; urd/registry/declaration.hpp), from which urd/registry/derived.hpp gives container_costs<R>, without defining
// R. Reading a representation's costs therefore completes nothing and includes no implementation:
// the registry's own headers suffice. The docs' complexity tables and the benchmarks' judgment of the registry
// (benchmarks/record.py) read these.

#include "urd/traits.hpp"

#include <concepts>
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

/// urd's operations: each its words, and the key of its entry in a container's costs (cost_table).
namespace operation
{

struct insert
{
    static constexpr std::string_view name = "insert";
    static constexpr std::string_view description = "a set's or map's insert, a sequence's push_back";
};
struct lookup
{
    static constexpr std::string_view name = "lookup";
    static constexpr std::string_view description = "find, contains, a map's value by key";
};
struct ordered_lookup
{
    static constexpr std::string_view name = "ordered_lookup";
    static constexpr std::string_view description = "lower_bound";
};
struct erase
{
    static constexpr std::string_view name = "erase";
    static constexpr std::string_view description = "by key, or by position";
};
struct clear
{
    static constexpr std::string_view name = "clear";
    static constexpr std::string_view description = "every element at once";
};
struct iterate
{
    static constexpr std::string_view name = "iterate";
    static constexpr std::string_view description = "every element in turn";
};
struct access
{
    static constexpr std::string_view name = "access";
    static constexpr std::string_view description = "by position or handle";
};
struct push
{
    static constexpr std::string_view name = "push";
    static constexpr std::string_view description = "a queue's";
};
struct pop
{
    static constexpr std::string_view name = "pop";
    static constexpr std::string_view description = "a queue's";
};
struct top
{
    static constexpr std::string_view name = "top";
    static constexpr std::string_view description = "a queue's";
};
struct merge
{
    static constexpr std::string_view name = "merge";
    static constexpr std::string_view description = "a partition's";
};
struct insert_row
{
    static constexpr std::string_view name = "insert_row";
    static constexpr std::string_view description = "a pool's insert";
};
struct find_row
{
    static constexpr std::string_view name = "find_row";
    static constexpr std::string_view description = "a pool's find";
};
struct scope
{
    static constexpr std::string_view name = "scope";
    static constexpr std::string_view description = "push_scope, changes, pop_scope";
};

}  // namespace operation

/// What operation Op costs in a container: one entry of its costs.
template<typename Op, cost Cost>
struct cost_entry
{
    using operation = Op;
    static constexpr cost value = Cost;
};

/// A container's costs: one entry per operation it offers. `of<Op>()` is Op's cost, or none where it has no entry.
template<typename... Entries>
struct cost_table
{
    template<typename Op>
    static constexpr std::optional<cost> of()
    {
        auto found = std::optional<cost>();
        ((std::same_as<Op, typename Entries::operation> ? void(found = Entries::value) : void()), ...);
        return found;
    }
    template<typename Op>
    static constexpr bool passed_on()
    { return false; }
};

/// A wrapper's costs: its own entries, and for every other operation its part's (Part, a declaration). An operation it
/// has no entry for is passed on to its part.
template<typename Part, typename... Entries>
struct costs_over
{
    using part = Part;
    template<typename Op>
    static constexpr std::optional<cost> of()
    {
        const auto own = cost_table<Entries...>::template of<Op>();
        return own ? own : Part::operation_costs::template of<Op>();
    }
    template<typename Op>
    static constexpr bool passed_on()
    { return !cost_table<Entries...>::template of<Op>() && Part::operation_costs::template of<Op>(); }
};

/// The costs of the operations of representation R: its declaration's operation_costs, a cost_table with an entry per
/// operation R offers (a wrapper's, costs_over its part's declaration). Its memory is the declaration's `words`: machine
/// words per element (the element counting 1; per key or slot for dense keys); what that memory holds is in words in its
/// synopsis (urd/registry/derived.hpp, container_synopsis). Declared for every registered representation, derived from
/// its declaration (urd/registry/derived.hpp); none here.
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
