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

/// The operations compared (urd::operation): which a Store offers follows from its facets.
using op = urd::operation;
/// Their names, in order (urd::name).
inline constexpr auto op_names = []
{
    auto result = std::array<std::string_view, static_cast<std::size_t>(op::count)> {};
    for (std::size_t each = 0; each < result.size(); ++each)
        result[each] = urd::name(static_cast<op>(each));
    return result;
}();

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

/// The cost of `what` in R (a container or a declaration), a wrapper's resolved through its part; none where R lacks
/// it.
template<typename R>
constexpr std::optional<cost> cost_of(op what)
{ return urd::cost_of<typename costs_holder<R>::type>(what); }

/// Whether R passes `what` on to its part: a wrapper's cost it inherits rather than declares (the same object).
template<typename R>
constexpr bool passed_on(op what)
{
    using C = typename costs_holder<R>::type;
    if constexpr (requires { typename C::part; })
    {
        using P = typename costs_holder<typename C::part>::type;
        switch (what)
        {
            case op::insert:
                if constexpr (requires { P::insert; })
                    return &C::insert == &P::insert;
                break;
            case op::lookup:
                if constexpr (requires { P::lookup; })
                    return &C::lookup == &P::lookup;
                break;
            case op::ordered_lookup:
                if constexpr (requires { P::ordered_lookup; })
                    return &C::ordered_lookup == &P::ordered_lookup;
                break;
            case op::erase:
                if constexpr (requires { P::erase; })
                    return &C::erase == &P::erase;
                break;
            case op::clear:
                if constexpr (requires { P::clear; })
                    return &C::clear == &P::clear;
                break;
            case op::iterate:
                if constexpr (requires { P::iterate; })
                    return &C::iterate == &P::iterate;
                break;
            case op::access:
                if constexpr (requires { P::access; })
                    return &C::access == &P::access;
                break;
            case op::push:
                if constexpr (requires { P::push; })
                    return &C::push == &P::push;
                break;
            case op::pop:
                if constexpr (requires { P::pop; })
                    return &C::pop == &P::pop;
                break;
            case op::top:
                if constexpr (requires { P::top; })
                    return &C::top == &P::top;
                break;
            case op::merge:
                if constexpr (requires { P::merge; })
                    return &C::merge == &P::merge;
                break;
            case op::insert_row:
                if constexpr (requires { P::insert_row; })
                    return &C::insert_row == &P::insert_row;
                break;
            case op::find_row:
                if constexpr (requires { P::find_row; })
                    return &C::find_row == &P::find_row;
                break;
            case op::scope:
                if constexpr (requires { P::scope; })
                    return &C::scope == &P::scope;
                break;
            case op::count:
                break;
        }
    }
    return false;
}

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

/// The operations a Store of R for S offers: those its facets offer (urd::detail::facet_operations, beside each
/// facet) where they are on.
template<typename R, typename S>
constexpr std::array<bool, static_cast<std::size_t>(op::count)> offered()
{
    auto result = std::array<bool, static_cast<std::size_t>(op::count)> {};
    const auto offer = [&]<template<typename, typename, typename> class F>()
    {
        if (urd::detail::facet_on<R, S, F>())
            for (const auto each : urd::detail::facet_operations<F>::offered)
                result[static_cast<std::size_t>(each)] = true;
    };
    []<template<typename, typename, typename> class... Fs>(urd::detail::facet_list<Fs...> /*facets*/, const auto& each_facet)
    { (each_facet.template operator()<Fs>(), ...); }(typename S::kind::facets {}, offer);
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
