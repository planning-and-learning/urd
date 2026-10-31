#pragma once

// Every registered container as a type, walked in the registry's order (urd/registry/preferences.hpp): whether each
// serves a request (serves<R, S>), and the first that does. Selection no longer walks (it scans the rows as values,
// urd/registry/select.hpp), so a file holding a Store never includes this; the tests, generators and benchmarks that
// read every container for a request do, which is linear on purpose. The rows mirror what serves<R, S> reads.

#include "urd/interface.hpp"
#include "urd/properties.hpp"
#include "urd/registry/features.hpp"
#include "urd/registry/preferences.hpp"
#include "urd/traits.hpp"

#include <type_traits>
#include <vector>

namespace urd::detail
{

/// Whether representation R can serve spec S: it declares its properties and has S's for its elements (a set
/// of elements without operator< is not sorted), and the interface a Store for S offers: it places elements as
/// S does (by value or where they are put), demands nothing S left open (a dense container needs the keys it was
/// promised), is unique exactly where S is (a unique container would drop a
/// sequence's duplicates), is reached by handle exactly where S is (a pool offers no iteration), stores records flat
/// exactly where S asks (records come back by index), is a map exactly
/// where S is, is a queue exactly where S is, and is random-access where S
/// is a sequence. A sequence
/// kept by value must be sorted, or its positions would mean nothing. Where S's elements (a map's values) are Stores (rows), R
/// holds them with their requested properties (element_traits).
template<typename R, typename S>
concept serves_by_traits = container_traits<R>::declared && detail::satisfies(container_traits<R>::urd_properties, requirement<S>)
                           && detail::demands_met(container_traits<R>::urd_properties, S::urd_properties) && by_value<container_traits<R>> == by_value<S>
                           && has<container_traits<R>, unique> == has<S, unique> && has<container_traits<R>, handles> == has<S, handles>
                           && has<container_traits<R>, flat> == has<S, flat> && (!S::is_sequence || !by_value<S> || has<S, sorted>)
                           && queue_traits<container_traits<R>> == S::is_queue;
// Selection reads only the traits, which are partial specializations matched on R's arguments: checking them
// instantiates no container, and a candidate need not even be defined where a request is resolved (the registry
// declares representations; the header a Store's family comes from defines them). Whether a container offers what
// its traits promise (conforming) is checked once, by the typed test suites, for every registered container, not
// again for every request: those interface checks were most of what resolving a request cost the compiler.
/// What S holds as rows where they are Stores: a map's values, else its elements.
template<typename S>
using row_of = std::conditional_t<S::is_map, typename S::mapped_type, typename S::element_type>;

/// The properties R gives S's rows: element_traits<R> where R declares them, else the rows' own but for memory
/// (destroying a row frees what it owns, as erase and clear do).
template<typename R, typename S>
struct row_traits
{
    using type = element_traits<R>;
};
template<typename R, typename S>
    requires requires { element_traits<R>::defaulted; }
struct row_traits<R, S>
{
    using type = with_t<container_traits<row_of<S>>, reclaims>;
};

template<typename R, typename S>
concept serves = serves_by_traits<R, S> && detail::promises(S::urd_properties, relies_on<R>) && (!has<S, positional> || has<container_traits<R>, positional>)
                 && (!is_store<row_of<S>> || detail::satisfies(row_traits<R, S>::type::urd_properties, requirement<container_traits<row_of<S>>>));

/// Stable storage: it keeps references stable, by a node per element or by segments that never move. Under
/// small or frozen these move last, as contiguous storage is faster for few or unchanging elements.
template<typename R>
inline constexpr bool stable_storage = has<container_traits<R>, stable_refs>;

/// The order for S: where elements are few or never change, containers with stable storage come last, since contiguous
/// storage is more compact and faster to search and iterate (docs/BENCHMARKS.md); otherwise the lists' order.
template<bool Compact, typename... Rs>
struct reordered
{
    using type = type_list<Rs...>;
};
// Built only where it is used: join is the costly part of choosing, and reading every candidate's traits is spared
// where the list stays as it is.
template<typename... Rs>
struct reordered<true, Rs...>
{
    using type = typename join<std::conditional_t<stable_storage<Rs>, type_list<>, type_list<Rs>>...,
                               std::conditional_t<stable_storage<Rs>, type_list<Rs>, type_list<>>...>::type;
};
template<typename S, typename List>
struct ordered_for;
template<typename S, typename... Rs>
struct ordered_for<S, type_list<Rs...>>
{
    using type = typename reordered<has<S, frozen> || has<S, small>, Rs...>::type;
};

/// The container chosen.
template<typename R>
struct chosen
{
    static constexpr bool found = true;
    using type = R;
};

/// The first of Rs that serves S, checked one by one and no further: of each conditional only the branch taken is
/// instantiated. Where none serves, a stand-in after the static_assert in choose_in, so that it is the only error.
template<typename S, typename... Rs>
struct first_serving
{
    static constexpr bool found = false;
    using type = std::vector<typename S::element_type>;
};
template<typename S, typename R, typename... Rs>
struct first_serving<S, R, Rs...> : std::conditional_t<serves<R, S>, chosen<R>, first_serving<S, Rs...>>
{
};

template<typename S, typename List>
struct choose_in;

/// The first of the containers that serves S, if any (found).
template<typename S, typename... Rs>
struct choose_in<S, type_list<Rs...>> : first_serving<S, Rs...>
{
};

/// The candidates for spec S: the comparator-taking ones where it gives order_by (those ordered by values too where
/// its P reads them), else all.
template<typename S>
using candidates_for = std::conditional_t<
    std::is_void_v<typename S::order_type>,
    std::conditional_t<S::is_map, instantiated<S, map_preference>, instantiated<S, element_preference>>,
    std::conditional_t<
        S::is_map,
        std::conditional_t<is_entry_less<typename S::order_type>, instantiated<S, value_ordered_map_preference>, instantiated<S, ordered_map_preference>>,
        instantiated<S, ordered_element_preference>>>;

/// The container the walk picks for spec S: the first registered one that serves it, in the order for S. What
/// choose<S> (urd/registry/select.hpp) picks by value; requests_group.cpp checks they are the same.
template<typename S>
using walk_choice = choose_in<S, typename ordered_for<S, typename candidates_for<S>::type>::type>;

}  // namespace urd::detail
