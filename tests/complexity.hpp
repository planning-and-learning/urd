#pragma once

// What each operation costs each representation, as data: the complexity sections of docs/kinds/*.rst are written
// from it (share/urd/complexity.json, tests/complexity_table.cpp), the registry's choices are judged by it from that
// file and the measured results (benchmarks/record.py), and tests/complexity_verify.cpp checks it by counting the
// work done.

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

/// The cost of `what` in R (urd::container_costs<R>, a wrapper's resolved through its part); none where R lacks it.
template<typename R>
constexpr std::optional<cost> cost_of(op what)
{
    using C = urd::container_costs<R>;
    switch (what)
    {
        case op::insert:
            if constexpr (requires { C::insert; })
                return C::insert;
            break;
        case op::lookup:
            if constexpr (requires { C::lookup; })
                return C::lookup;
            break;
        case op::ordered_lookup:
            if constexpr (requires { C::ordered_lookup; })
                return C::ordered_lookup;
            break;
        case op::erase:
            if constexpr (requires { C::erase; })
                return C::erase;
            break;
        case op::clear:
            if constexpr (requires { C::clear; })
                return C::clear;
            break;
        case op::iterate:
            if constexpr (requires { C::iterate; })
                return C::iterate;
            break;
        case op::access:
            if constexpr (requires { C::access; })
                return C::access;
            break;
        case op::push:
            if constexpr (requires { C::push; })
                return C::push;
            break;
        case op::pop:
            if constexpr (requires { C::pop; })
                return C::pop;
            break;
        case op::top:
            if constexpr (requires { C::top; })
                return C::top;
            break;
        case op::merge:
            if constexpr (requires { C::merge; })
                return C::merge;
            break;
        case op::insert_row:
            if constexpr (requires { C::insert_row; })
                return C::insert_row;
            break;
        case op::find_row:
            if constexpr (requires { C::find_row; })
                return C::find_row;
            break;
        case op::scope:
            if constexpr (requires { C::scope; })
                return C::scope;
            break;
        case op::count:
            break;
    }
    return std::nullopt;
}

/// Whether R passes `what` on to its part: a wrapper's cost it inherits rather than declares (the same object).
template<typename R>
constexpr bool passed_on(op what)
{
    using C = urd::container_costs<R>;
    if constexpr (requires { typename C::part; })
    {
        using P = urd::container_costs<typename C::part>;
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

/// The part R wraps, by name: the one its costs are passed on to, else the first of urd::parts<R>; empty where R
/// wraps none.
template<typename R>
std::string_view part_name()
{
    if constexpr (requires { typename urd::container_costs<R>::part; })
        return name<typename urd::container_costs<R>::part>::id;
    else if constexpr (requires { typename urd::part_of<R>; })
        return name<urd::part_of<R>>::id;
    else
        return {};
}

/// R's memory in words per element; none where it declares none.
template<typename R>
constexpr std::optional<double> words_of()
{
    if constexpr (requires { urd::container_costs<R>::words; })
        return urd::container_costs<R>::words;
    else
        return std::nullopt;
}

/// The guarantees that cost something to give: storage by index, nodes, locks, generations, a log, a parent, shared
/// parts. A container that gives one nobody asked for is a surprise, unlike one that also happens to keep its memory,
/// lie contiguous, or iterate in order (a dense container's order is its index order, at no cost).
inline constexpr std::array costly { urd::tag::dense,       urd::tag::stable_refs,  urd::tag::concurrent, urd::tag::constant_clear,     urd::tag::scoped,
                                     urd::tag::layered,     urd::tag::change_log,   urd::tag::handles,    urd::tag::structural_sharing, urd::tag::partition,
                                     urd::tag::fixed_shape, urd::tag::bidirectional };

/// The facets the operations below read, from the spec alone: the cases of urd::detail::facet_switches that do not
/// read the representation (only equality and ordering do, and they are not compared). Each container's benchmark
/// checks it against facet_switches for its own Store (benchmarks/experiment.cpp).
template<typename S>
struct spec_switches
{
    static constexpr bool on(urd::detail::facet_id id)
    {
        using urd::has;
        using urd::detail::facet_id;
        switch (id)
        {
            case facet_id::iteration:
                return S::iterable || (S::is_queue && !has<S, urd::monotone_priority>);
            case facet_id::lookup:
                return urd::by_value<S> && S::iterable;
            case facet_id::ordered_lookup:
                return has<S, urd::sorted> && !urd::detail::is_entry_less<typename S::order_type>;
            case facet_id::positions:
                return has<S, urd::positional> || (S::is_set && has<S, urd::insertion_ordered> && !has<S, urd::reclaims>);
            case facet_id::growth:
                return has<S, urd::growable> && S::iterable;
            case facet_id::removal:
                return has<S, urd::shrinkable> && S::iterable;
            case facet_id::values_by_key:
                return S::is_map && has<S, urd::unique>;
            case facet_id::multi_values:
                return S::is_map && !has<S, urd::unique>;
            case facet_id::queue_access:
                return S::is_queue;
            case facet_id::scopes:
                return has<S, urd::scoped>;
            case facet_id::pool:
                return S::is_pool && has<S, urd::unique> && !has<S, urd::flat>;
            case facet_id::records:
                return S::is_pool && has<S, urd::unique> && has<S, urd::flat>;
            case facet_id::slots:
                return S::is_pool && !has<S, urd::unique>;
            case facet_id::grouping:
                return S::is_partition;
            case facet_id::shape:
                return has<S, urd::fixed_shape>;
            default:
                return false;
        }
    }
};

/// The operations a Store offers, read from its facets (`On::on`); the facets not listed here (sizing, equality, set
/// algebra, ...) are not compared.
template<typename S, typename On>
constexpr std::array<bool, static_cast<std::size_t>(op::count)> offered_by()
{
    using urd::detail::facet_id;
    auto result = std::array<bool, static_cast<std::size_t>(op::count)> {};
    const auto offer = [&](op what) { result[static_cast<std::size_t>(what)] = true; };
    // A fixed shape is set at construction: the growth facet stays on for its rows, but insert is denied.
    if (On::on(facet_id::growth) && !urd::has<S, urd::fixed_shape>)
        offer(op::insert);
    if (On::on(facet_id::removal))
    {
        offer(op::erase);
        offer(op::clear);
    }
    if (On::on(facet_id::lookup) || On::on(facet_id::values_by_key) || On::on(facet_id::multi_values))
        offer(op::lookup);
    if (On::on(facet_id::ordered_lookup))
        offer(op::ordered_lookup);
    if (On::on(facet_id::iteration))
        offer(op::iterate);
    if (On::on(facet_id::positions) || On::on(facet_id::shape))
        offer(op::access);
    if (On::on(facet_id::queue_access))
    {
        offer(op::push);
        offer(op::pop);
        offer(op::top);
        offer(op::clear);
    }
    if (On::on(facet_id::grouping))
    {
        offer(op::merge);
        offer(op::lookup);
    }
    if (On::on(facet_id::pool) || On::on(facet_id::records))
    {
        offer(op::insert_row);
        offer(op::find_row);
        offer(op::access);
    }
    if (On::on(facet_id::slots))
    {
        offer(op::insert_row);
        offer(op::access);
    }
    if (On::on(facet_id::scopes))
        offer(op::scope);
    return result;
}

/// The operations a Store of R for S offers.
template<typename R, typename S>
constexpr std::array<bool, static_cast<std::size_t>(op::count)> offered()
{ return offered_by<S, urd::detail::facet_switches<R, S>>(); }

/// The operations a request's Store offers, from its spec alone (spec_switches).
template<typename S>
constexpr std::array<bool, static_cast<std::size_t>(op::count)> offered()
{ return offered_by<S, spec_switches<S>>(); }

}  // namespace urd::test
