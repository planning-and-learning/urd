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
#include "urd/elements/prehashed.hpp"
#include "urd/store.hpp"

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

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
    static constexpr bool on_facet = requires(B& facet, element_of<S>&& element) { facet.emplace(std::move(element)); };
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
    static constexpr bool on_facet = requires(B& facet, element_of<S>&& element) { facet.push_back(std::move(element)); };
};

/// assign copies: only a copyable element (takes<facet::assigning> below).
template<>
struct probe<facet::assigning>
{
    template<typename T, typename R, typename S>
    static constexpr bool also = std::copy_constructible<element_of<S>>;
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

// Moving and copying (docs/semantics.rst): a facet's members that put an element (or a key) in move it in by their &&
// forms, accepted wherever the facet offers them, and copy it in by their const&, range and initializer_list forms,
// accepted exactly where it is copyable; for a move-only one they are deleted (urd::detail::denied). `takes<F>` names a
// facet's moving calls (`moves`) and how many of its copying calls a facet base accepts (`copies`, of `copying`), for
// the element or key it takes (`taken`), where the facet's member is offered (`offered`); `move` makes the moving calls
// (a pool of records: its interning calls), so that they are compiled, not only resolved.

/// A facet that takes nothing names no calls.
template<template<typename, typename, typename> class F>
struct takes
{
    static constexpr bool any = false;
};

/// What a facet that takes elements has unless it says otherwise: it takes the element, and offers its members wherever
/// it is on.
struct takes_defaults
{
    static constexpr bool any = true;
    /// It interns (reads what it is given) rather than taking it: its copying calls are accepted for any element.
    static constexpr bool interns = false;
    template<typename S>
    using taken = element_of<S>;
    template<typename T, typename R, typename S>
    static constexpr bool offered = true;
};

template<>
struct takes<facet::insertion> : takes_defaults
{
    template<typename B, typename S>
    static constexpr bool moves = requires(B& facet, element_of<S>&& element) { facet.insert(std::move(element)); };
    static constexpr int copying = 3;
    template<typename B, typename S>
    static constexpr int copies = int { requires(B& facet, const element_of<S>& element) { facet.insert(element); } }
                                  + int { requires(B& facet, std::initializer_list<element_of<S>> elements) { facet.insert(elements); } }
                                  + int { requires(B& facet, const element_of<S>* first) { facet.insert(first, first); } };

    /// A map whose key is move-only takes its keys by values_by_key's try_emplace.
    template<typename B, typename S>
    static void move(B& facet, element_of<S>&& element)
    {
        if constexpr (std::move_constructible<element_of<S>>)
            facet.insert(std::move(element));
    }
};

template<>
struct takes<facet::appending> : takes_defaults
{
    template<typename B, typename S>
    static constexpr bool moves = requires(B& facet, element_of<S>&& element) { facet.push_back(std::move(element)); };
    static constexpr int copying = 1;
    template<typename B, typename S>
    static constexpr int copies = int { requires(B& facet, const element_of<S>& element) { facet.push_back(element); } };

    template<typename B, typename S>
    static void move(B& facet, element_of<S>&& element)
    { facet.push_back(std::move(element)); }
};

/// Assigning only copies: a Store of move-only elements is assigned another Store, moved.
template<>
struct takes<facet::assigning> : takes_defaults
{
    template<typename B, typename S>
    static constexpr bool moves = true;
    static constexpr int copying = 2;
    template<typename B, typename S>
    static constexpr int copies = int { requires(B& facet, const element_of<S>& element) { facet.assign(std::size_t {}, element); } }
                                  + int { requires(B& facet, const element_of<S>* first) { facet.assign(first, first); } };

    template<typename B, typename S>
    static void move(B& /*facet*/, const element_of<S>& /*element*/)
    {
    }
};

template<>
struct takes<facet::queue_access> : takes_defaults
{
    template<typename B, typename S>
    static constexpr bool moves = requires(B& facet, element_of<S>&& element) { facet.push(std::move(element)); };
    static constexpr int copying = 1;
    template<typename B, typename S>
    static constexpr int copies = int { requires(B& facet, const element_of<S>& element) { facet.push(element); } };

    template<typename B, typename S>
    static void move(B& facet, element_of<S>&& element)
    { facet.push(std::move(element)); }
};

/// Slots acquired by key (urd::keyed<K>) take no element.
template<>
struct takes<facet::slots> : takes_defaults
{
    template<typename T, typename R, typename S>
    static constexpr bool offered = std::is_void_v<typename S::slot_key_type>;
    template<typename B, typename S>
    static constexpr bool moves = requires(B& facet, element_of<S>&& element) { facet.insert(std::move(element)); };
    static constexpr int copying = 1;
    template<typename B, typename S>
    static constexpr int copies = int { requires(B& facet, const element_of<S>& element) { facet.insert(element); } };

    template<typename B, typename S>
    static void move(B& facet, element_of<S>&& element)
    { facet.insert(std::move(element)); }
};

/// A pool of records interns: it reads a record (a const T&, or one insert(key, make)'s make refers to), which the arena
/// serializes and a by-value interner copies (a copyable record; the calls resolve for any record, and a move-only one
/// fails at its deleted copy constructor). insert(T&&) and a make that gives a T hand the record over.
template<>
struct takes<facet::records> : takes_defaults
{
    static constexpr bool interns = true;
    template<typename S>
    using refers = const element_of<S>& (*) (typename S::handle_type);
    template<typename S>
    using gives = element_of<S> (*)(typename S::handle_type);
    template<typename B, typename S>
    static constexpr bool moves = requires(B& facet, element_of<S>&& element, const element_of<S>& key, gives<S> make) {
        facet.insert(std::move(element));
        facet.insert(key, make);
    };
    static constexpr int copying = 3;
    template<typename B, typename S>
    static constexpr int copies = int { requires(B& facet, const element_of<S>& element) { facet.insert(element); } }
                                  + int { requires(B& facet, const urd::prehashed<element_of<S>>& key) { facet.insert(key); } }
                                  + int { requires(B& facet, const element_of<S>& key, refers<S> make) { facet.insert(key, make); } };

    template<typename B, typename S>
    static void move(B& facet, element_of<S>&& element)
    {
        if constexpr (std::copy_constructible<element_of<S>>)
        {
            const auto& record = element;
            facet.insert(record);
            facet.insert(record, [&record](auto) -> const element_of<S>& { return record; });
        }
        facet.insert(std::move(element));
    }
};

/// A map with unique keys stores a key where it is new.
template<>
struct takes<facet::values_by_key> : takes_defaults
{
    template<typename S>
    using taken = typename S::key_type;
    template<typename T, typename R, typename S>
    static constexpr bool offered = urd::detail::grows<S>;
    template<typename B, typename S>
    static constexpr bool moves = requires(B& facet, typename S::key_type&& key) { facet.try_emplace(std::move(key)); };
    static constexpr int copying = 1;
    template<typename B, typename S>
    static constexpr int copies = int { requires(B& facet, const typename S::key_type& key) { facet.try_emplace(key); } };

    template<typename B, typename S>
    static void move(B& facet, typename S::key_type&& key)
    { facet.try_emplace(std::move(key)); }
};

/// Whether facet F of Store T (over R, for S) takes elements as the rule says: where it offers its members, the moving
/// calls are accepted (where the element can be moved: a map's element holds its key const), and the copying calls all
/// where what it takes is copyable or the facet interns it, none where it is move-only.
template<typename T, typename R, typename S, template<typename, typename, typename> class F>
constexpr bool takes_by_the_rule()
{
    if constexpr (takes<F>::any)
    {
        if constexpr (urd::detail::facet_condition<F>::template on<R, S>() && takes<F>::template offered<T, R, S>)
        {
            using B = F<T, R, S>;
            using taken = typename takes<F>::template taken<S>;
            return takes<F>::template moves<B, S> == std::move_constructible<taken>
                   && takes<F>::template copies<B, S> == (takes<F>::interns || std::copy_constructible<taken> ? takes<F>::copying : 0);
        }
    }
    // A facet that takes nothing, or does not offer its members here, takes them by the rule.
    return true;
}

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
