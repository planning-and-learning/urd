#pragma once

// What each operation costs each representation, as data: the complexity sections of docs/kinds/*.rst are written
// from it (share/urd/complexity.json, tests/complexity_table.cpp), the registry's choices are judged by it from that
// file and the measured results, which also check each cost's growth (benchmarks/record.py).

#include "names.hpp"
#include "urd/costs.hpp"
#include "urd/store/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace urd::test
{

/// The operations compared: urd's (urd::operation), by their position here, the order the tables list them in.
enum class op : std::uint8_t
{
    insert,
    lookup,
    ordered_lookup,
    erase,
    clear,
    iterate,
    access,
    push,
    pop,
    top,
    merge,
    insert_row,
    find_row,
    scope,
    count  ///< not an operation: how many there are
};
using operations = urd::operation_list<urd::operation::insert, urd::operation::lookup, urd::operation::ordered_lookup, urd::operation::erase, urd::operation::clear, urd::operation::iterate, urd::operation::access, urd::operation::push, urd::operation::pop, urd::operation::top, urd::operation::merge, urd::operation::insert_row, urd::operation::find_row, urd::operation::scope>;
/// Their names, in order.
inline constexpr auto op_names = []<typename... Ops>(urd::operation_list<Ops...> /*operations*/)
{ return std::array<std::string_view, sizeof...(Ops)> { Ops::name... }; }(operations {});
static_assert(op_names.size() == static_cast<std::size_t>(op::count));
/// The op of operation `of`.
constexpr op op_of(const urd::operation_info* of)
{
    const auto infos = []<typename... Ops>(urd::operation_list<Ops...> /*operations*/)
    { return std::array<const urd::operation_info*, sizeof...(Ops)> { urd::operation_v<Ops>... }; }(operations {});
    for (std::size_t each = 0; each < infos.size(); ++each)
        if (infos[each] == of)
            return static_cast<op>(each);
    throw "urd: an operation the tables do not list (tests/complexity.hpp, operations)";
}

/// Which operations a Store offers, by op.
using offers = std::array<bool, static_cast<std::size_t>(op::count)>;

/// The cost types are the library's (urd/costs.hpp), where each representation declares its costs.
using urd::cost;
using urd::growth;
using in = urd::quantity;

/// The costs of X: a declaration's own (operation_costs), else a container's (urd::container_costs, derived from its
/// declaration).
template<typename X>
struct costs_holder
{
    using type = urd::container_costs<X>;
};
template<typename X>
    requires requires { typename X::operation_costs; }
struct costs_holder<X>
{
    using type = typename X::operation_costs;
};

/// The costs C states, and whether C passes each on from P, in the order of `operations`: one array per container.
template<typename C, typename... Ops>
constexpr std::array<std::optional<cost>, sizeof...(Ops)> costs_in(urd::operation_list<Ops...> /*operations*/)
{ return { Ops::template in<C>()... }; }
template<typename C, typename P, typename... Ops>
constexpr std::array<bool, sizeof...(Ops)> passed_in(urd::operation_list<Ops...> /*operations*/)
{ return { Ops::template passed_on<C, P>()... }; }
template<typename R>
inline constexpr auto costs_v = costs_in<typename costs_holder<R>::type>(operations {});
template<typename R>
consteval std::array<bool, static_cast<std::size_t>(op::count)> passed_of()
{
    using C = typename costs_holder<R>::type;
    if constexpr (requires { typename C::part; })
        return passed_in<C, typename costs_holder<typename C::part>::type>(operations {});
    else
        return {};
}
template<typename R>
inline constexpr auto passed_v = passed_of<R>();

/// The cost of `what` in R (a container or a declaration), a wrapper's resolved through its part; none where R lacks
/// it.
template<typename R>
constexpr std::optional<cost> cost_of(op what)
{ return costs_v<R>[static_cast<std::size_t>(what)]; }

/// Whether R passes `what` on to its part: a wrapper's cost it inherits rather than declares (the same object).
template<typename R>
constexpr bool passed_on(op what)
{ return passed_v<R>[static_cast<std::size_t>(what)]; }

/// The part R wraps, by name: the declaration its costs are passed on to; empty where R wraps none.
template<typename R>
std::string_view part_name()
{
    if constexpr (requires { typename costs_holder<R>::type::part; })
        return costs_holder<R>::type::part::id;
    else
        return {};
}

/// R's memory in words per element; none where it declares none.
template<typename R>
constexpr std::optional<double> words_of()
{
    if constexpr (requires { costs_holder<R>::type::words; })
        return costs_holder<R>::type::words;
    else
        return std::nullopt;
}

/// The guarantees that cost something to give: storage by index, nodes, locks, generations, a log, a parent, shared
/// parts. A container that gives one nobody asked for is a surprise, unlike one that also happens to keep its memory,
/// lie contiguous, or iterate in order (a dense container's order is its index order, at no cost).
inline constexpr std::array<urd::tag, 12> costly { urd::keys::dense,       urd::referencing::stable_refs,  urd::threading::concurrent, urd::clearing::constant_clear,     urd::scoping::scoped,
                                     urd::layering::layered,     urd::tracking::change_log,   urd::handling::handles,    urd::sharing::structural_sharing, urd::grouping::partition,
                                     urd::shape::fixed_shape, urd::direction::bidirectional };

/// The ops of operations Ops, by position.
template<typename... Ops>
constexpr std::array<op, sizeof...(Ops)> ops_of(urd::operation_list<Ops...> /*operations*/)
{ return { op_of(urd::operation_v<Ops>)... }; }
template<typename List>
inline constexpr auto ops_v = ops_of(List {});
/// Marks the operations of facet F of a Store of R for S where it is on, and of all its facets.
template<typename R, typename S, template<typename, typename, typename> class F>
constexpr void mark_facet(offers& into)
{
    if constexpr (urd::detail::facet_on<R, S, F>())
        for (const auto each : ops_v<typename urd::detail::facet_operations<F>::offered>)
            into[static_cast<std::size_t>(each)] = true;
}
template<typename R, typename S, template<typename, typename, typename> class... Fs>
constexpr void mark_offered(urd::detail::facet_list<Fs...> /*facets*/, offers& into)
{ (mark_facet<R, S, Fs>(into), ...); }

/// The operations a Store of R for S offers: those its facets offer (urd::detail::facet_operations, beside each
/// facet) where they are on.
template<typename R, typename S>
constexpr std::array<bool, static_cast<std::size_t>(op::count)> offered()
{
    auto result = std::array<bool, static_cast<std::size_t>(op::count)> {};
    mark_offered<R, S>(typename S::kind::facets {}, result);
    // A fixed shape is set at construction: the growth facet stays on for its rows, but insert is denied.
    if (urd::has<S, urd::fixed_shape>)
        result[static_cast<std::size_t>(op::insert)] = false;
    return result;
}

/// The operations a request's Store offers, from its spec alone: no container is named (R is void).
template<typename S>
constexpr std::array<bool, static_cast<std::size_t>(op::count)> offered()
{ return offered<void, S>(); }

}  // namespace urd::test
