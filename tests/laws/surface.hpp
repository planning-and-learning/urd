#pragma once

// A Store's surface: for each facet (include/urd/store/), a probe, a call of one of its members. tests/laws/surface_group.cpp
// holds every request of the grid (tests/support/grid.hpp) to them: the member is callable exactly where the facet is on (its
// condition, urd::detail::facet_condition) and the member's own condition holds (`also`, where the member asks more of
// its container than the facet does).
//
// A probe calls its member on the facet's own base (F<Store, R, S>), which a Store inherits where the facet's condition
// holds: a member another facet of the kind also names (front in ends and in queue_access) is the facet's, and a facet
// a Store does not inherit is never instantiated for it. Hidden friends (==,
// <=>) are called on the Store: no other facet declares them.

#include "urd/elements/concepts.hpp"
#include "urd/store.hpp"

#include <cstddef>
#include <initializer_list>

namespace urd::test
{

namespace facet = urd::detail::facet;

/// A facet's probe: `on_facet<B, T, S>`, whether the facet's base B of Store T (for spec S) can make the call, or `on_store<T, S>`,
/// whether Store T can; and `also<T, R, S>`, what the member asks beyond the facet's condition. A facet without one is
/// named in unprobed below; any other fails to compile, so a new facet adds its probe.
template<template<typename, typename, typename> class F>
struct probe;

/// A probe whose member asks nothing beyond the facet's condition.
struct probe_defaults
{
    template<typename T, typename R, typename S>
    static constexpr bool also = true;
};

/// The lookup key and the element of spec S, as a probe passes them.
template<typename S>
using key_of = typename urd::detail::store_types<S>::lookup_type;
template<typename S>
using element_of = typename urd::detail::store_types<S>::value_type;

template<>
struct probe<facet::sizing> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet) { facet.size(); };
};
template<>
struct probe<facet::equality>
{
    template<typename T, typename S>
    static constexpr bool on_store = requires(const T& store) { store == store; };
    template<typename T, typename R, typename S>
    static constexpr bool also = urd::EqualityComparableByEqualTo<R>;
};
template<>
struct probe<facet::ordering>
{
    template<typename T, typename S>
    static constexpr bool on_store = requires(const T& store) { store <=> store; };
    /// Its elements ordered by urd::Less: asked of the Store's own iteration (a flattened Store's rows are slices).
    template<typename T, typename R, typename S>
    static constexpr bool also = urd::OrderedByLess<std::ranges::range_value_t<T>>;
};
template<>
struct probe<facet::ordered_lookup> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet, const key_of<S>& key) { facet.lower_bound(key); };
};
template<>
struct probe<facet::contiguous_data> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet) { facet.data(); };
};
template<>
struct probe<facet::reserving>
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet) { facet.reserve(std::size_t {}); };
    template<typename T, typename R, typename S>
    static constexpr bool also = urd::has<R, urd::reservable>;
};
template<>
struct probe<facet::scopes> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet) { facet.push_scope(); };
};
template<>
struct probe<facet::layers> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet) { facet.parent(); };
};
template<>
struct probe<facet::shape> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet) { facet.reset(); };
};
template<>
struct probe<facet::ends> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet) { facet.front(); };
};
template<>
struct probe<facet::iteration> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet) { facet.begin(); };
};
template<>
struct probe<facet::lookup> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet, const key_of<S>& key) { facet.find(key); };
};
template<>
struct probe<facet::positions> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet, typename B::position_type position) { facet.at(position); };
};
template<>
struct probe<facet::insertion_positions> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet, typename B::index_type index) { facet.at(index); };
};
template<>
struct probe<facet::insertion> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet, std::initializer_list<element_of<S>> elements) { facet.insert(elements); };
};
template<>
struct probe<facet::removal> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet) { facet.clear(); };
};
template<>
struct probe<facet::appending> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet, const element_of<S>& element) { facet.push_back(element); };
};
template<>
struct probe<facet::assigning> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet, const element_of<S>& element) { facet.assign(std::size_t {}, element); };
};
template<>
struct probe<facet::values_by_key> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet, const key_of<S>& key) { facet.at(key); };
};
template<>
struct probe<facet::entries> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet) { facet.values(); };
};
template<>
struct probe<facet::inverse> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet, const typename S::mapped_type& value) { facet.keys(value); };
};
template<>
struct probe<facet::graph> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet) { facet.num_vertices(); };
};
template<>
struct probe<facet::graph_building>
{
    /// clear() where the Store may be cleared (not monotone).
    template<typename T, typename R, typename S>
    static constexpr bool also = urd::detail::clears<S>;
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet) { facet.clear(); };
};
template<>
struct probe<facet::graph_removal> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet) { facet.remove_edge(std::size_t {}); };
};
template<>
struct probe<facet::graph_edges> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet) { facet.num_edges(); };
};
template<>
struct probe<facet::graph_parts> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet) { facet.num_parts(); };
};
template<>
struct probe<facet::grouping> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet, const key_of<S>& key) { facet.contains(key); };
};
template<>
struct probe<facet::queue_access> : probe_defaults
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(B& facet) { facet.pop(); };
};
template<>
struct probe<facet::records>
{
    /// begin() where its records are not layered (a layered pool is iterated through the layer that owns a record).
    template<typename T, typename R, typename S>
    static constexpr bool also = !urd::has<S, urd::layered>;
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet) { facet.begin(); };
};
template<>
struct probe<facet::set_algebra>
{
    template<typename B, typename T, typename S>
    static constexpr bool on_facet = requires(const B& facet, const T& other) { facet.includes(other); };
    template<typename T, typename R, typename S>
    static constexpr bool also = urd::has<R, urd::dense, urd::canonical>;
};

/// The facets without a probe, and why: visiting and multi_values have no member; pool's and slots' calls take what
/// they hold (rows, handles), which the grid's requests do not give a probe.
template<template<typename, typename, typename> class F>
inline constexpr bool unprobed = false;
template<>
inline constexpr bool unprobed<facet::visiting> = true;
template<>
inline constexpr bool unprobed<facet::multi_values> = true;
template<>
inline constexpr bool unprobed<facet::pool> = true;
template<>
inline constexpr bool unprobed<facet::slots> = true;

/// Whether Store T (over R, for S) can make facet F's call: on the Store, or on the facet's base where the Store
/// inherits it (its condition holds), else not.
template<typename T, typename R, typename S, template<typename, typename, typename> class F>
constexpr bool callable()
{
    if constexpr (requires { probe<F>::template on_store<T, S>; })
        return probe<F>::template on_store<T, S>;
    else if constexpr (urd::detail::facet_condition<F>::template on<R, S>())
        return probe<F>::template on_facet<F<T, R, S>, T, S>;
    else
        return false;
}

/// Whether facet F of Store T (over R, for S) agrees with its probe: its member callable exactly where the facet is on
/// and the member's own condition holds. A facet without a probe agrees.
template<typename T, typename R, typename S, template<typename, typename, typename> class F>
constexpr bool agrees()
{
    if constexpr (unprobed<F>)
        return true;
    else if constexpr (!urd::detail::facet_condition<F>::template on<R, S>())
        return !callable<T, R, S, F>();
    else
        return probe<F>::template also<T, R, S> == callable<T, R, S, F>();
}

}  // namespace urd::test
