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

template<typename Lists>
struct offered_by;
template<template<typename, typename, typename> class... Fs>
struct offered_by<urd::detail::facet_list<Fs...>>
{
    template<typename Acc, typename... Lists>
    struct all
    {
        using type = Acc;
    };
    template<typename Acc, typename List, typename... Lists>
    struct all<Acc, List, Lists...> : all<typename append_list<Acc, List>::type, Lists...>
    {
    };
    using type = typename all<urd::operation_list<>, typename urd::detail::facet_operations<Fs>::offered...>::type;
};
template<typename Kind>
struct operations_of_kind
{
    using type = typename offered_by<urd::detail::facets_of<Kind>>::type;
};

/// The operations compared: every operation the facets of urd's kinds offer (urd::operation), in first-seen order, the
/// order the tables list them in.
using operations = union_of<urd::operation_list<>, operations_of_kind, urd::kinds>::type;
/// An operation by its position in `operations`; op_v<urd::operation::insert> names one.
enum class op : std::uint8_t
{
};
inline constexpr std::size_t op_count = []<typename... Ops>(urd::operation_list<Ops...> /*operations*/) { return sizeof...(Ops); }(operations {});
template<typename Operation>
inline constexpr op op_v = []<typename... Ops>(urd::operation_list<Ops...> /*operations*/)
{
    std::size_t at = 0;
    const bool found = ((std::is_same_v<Ops, Operation> || (++at, false)) || ...);
    if (!found)
        throw "urd: an operation no kind's facet offers (tests/complexity.hpp, operations)";
    return static_cast<op>(at);
}(operations {});
/// Their names, in order.
inline constexpr auto op_names = []<typename... Ops>(urd::operation_list<Ops...> /*operations*/)
{ return std::array<std::string_view, sizeof...(Ops)> { Ops::name... }; }(operations {});
/// Which operations a Store offers, by op.
using offers = std::array<bool, op_count>;

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
{ return { C::template of<Ops>()... }; }
template<typename C, typename... Ops>
constexpr std::array<bool, sizeof...(Ops)> passed_in(urd::operation_list<Ops...> /*operations*/)
{ return { C::template passed_on<Ops>()... }; }
template<typename R>
inline constexpr auto costs_v = costs_in<typename costs_holder<R>::type>(operations {});
template<typename R>
inline constexpr auto passed_v = passed_in<typename costs_holder<R>::type>(operations {});

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
    if constexpr (requires { R::words; })
        return R::words;
    else if constexpr (requires { urd::detail::declaration_t<R>::words; })
        return urd::detail::declaration_t<R>::words;
    else
        return std::nullopt;
}

/// The guarantees that cost something to give (a tag says so: urd::tag_defaults, costly), of every category of the
/// vocabulary.
inline constexpr auto costly = []
{
    constexpr auto count = []
    {
        std::size_t result = 0;
        for_each_tag(
            [&result](urd::tag each)
            {
                if (each.in->says[each.answer].costly)
                    ++result;
            });
        return result;
    }();
    auto result = std::array<urd::tag, count> {};
    std::size_t at = 0;
    for_each_tag(
        [&](urd::tag each)
        {
            if (each.in->says[each.answer].costly)
                result[at++] = each;
        });
    return result;
}();

/// The ops of operations Ops, by position.
template<typename... Ops>
constexpr std::array<op, sizeof...(Ops)> ops_of(urd::operation_list<Ops...> /*operations*/)
{ return { op_v<Ops>... }; }
template<typename List>
inline constexpr auto ops_v = ops_of(List {});
/// Marks the operations of facet F of a Store of R for S where it is on, and of all its facets.
template<typename R, typename S, template<typename, typename, typename> class F>
constexpr void mark_facet(offers& into)
{
    if constexpr (urd::detail::facet_condition<F>::template on<R, S>())
        for (const auto each : ops_v<typename urd::detail::facet_operations<F>::offered>)
            into[static_cast<std::size_t>(each)] = true;
}
template<typename R, typename S, template<typename, typename, typename> class... Fs>
constexpr void mark_offered(urd::detail::facet_list<Fs...> /*facets*/, offers& into)
{ (mark_facet<R, S, Fs>(into), ...); }

/// The operations a Store of R for S offers: those its facets offer (urd::detail::facet_operations, beside each
/// facet) where they are on.
template<typename R, typename S>
constexpr std::array<bool, op_count> offered()
{
    auto result = std::array<bool, op_count> {};
    mark_offered<R, S>(urd::detail::facets_of<typename S::kind> {}, result);
    return result;
}

/// The operations a request's Store offers, from its spec alone: no container is named (R is void).
template<typename S>
constexpr std::array<bool, op_count> offered()
{ return offered<void, S>(); }

}  // namespace urd::test
