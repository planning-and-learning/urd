#pragma once

// A grid of requests, each well-formed, over a few element types: what the resolution table (docs/RESOLUTION.md,
// written and checked by tests/resolution_table.cpp) and the registry audit (tests/registry.cpp) run over. Every
// request tests/survey.cpp pins has its shape here. Nothing instantiates a Store: a request's spec and its resolution read only
// traits.

#include "elements.hpp"
#include "names.hpp"
#include "urd/flat.hpp"
#include "urd/format.hpp"
#include "urd/registry.hpp"
#include "urd/request.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::test
{

/// Projections for order_by: by the negated element, and a map's entries by their values.
inline constexpr auto negated = [](int element) { return -element; };
inline constexpr auto by_value = [](const int& /*key*/, const int& value) { return value; };

// Requests as types, and the families a Store needs, come from the library (urd/request.hpp); a request is written as
// urd::spelling_of_request says (urd/format.hpp).
using urd::cat_t;
using urd::element_families_of;
using urd::families_of;
using urd::for_each_request;
using urd::requests;

/// One request of the grid: urd::request, with how it is written.
template<typename... Ps>
struct request : urd::request<Ps...>
{
    static std::string describe() { return urd::spelling_of_request<urd::request<Ps...>>(); }
};

// The groups. Unservable requests are in: the table shows "none" for them, which is what they are today (concurrent
// sets and maps by hashed keys among them in a build without gtl, which is why the table is checked only with it).

template<typename K>
using sets_of = requests<request<key<K>>,
                         request<key<K>, sorted>,
                         request<key<K>, sorted, frozen>,
                         request<key<K>, frozen>,
                         request<key<K>, small>,
                         request<key<K>, stable_refs>,
                         request<key<K>, small, stable_refs>,
                         request<key<K>, sorted, stable_refs>,
                         request<key<K>, small, sorted, stable_refs>,
                         request<key<K>, insertion_ordered>,
                         request<key<K>, positional>,
                         request<key<K>, positional, sorted>,
                         request<key<K>, positional, monotone>,
                         request<key<K>, positional, monotone, stable_refs>,
                         request<key<K>, positional, monotone, concurrent>,
                         request<key<K>, concurrent>,
                         request<key<K>, stable_refs, concurrent>,
                         request<key<K>, retains>,
                         request<key<K>, sorted, retains>,
                         request<key<K>, stable_refs, retains>,
                         request<key<K>, sorted, stable_refs, retains>,
                         request<key<K>, reclaims>,
                         request<key<K>, sorted, reclaims>,
                         request<key<K>, insertion_ordered, reclaims>,
                         request<key<K>, scoped>,
                         request<key<K>, positional, monotone, layered>,
                         request<key<K>, change_log, concurrent>,
                         request<key<K>, layered>,
                         request<key<K>, stable_refs, layered>,
                         request<key<K>, change_log>,
                         request<key<K>, sorted, change_log>,
                         request<key<K>, stable_refs, change_log>,
                         request<key<K>, nonempty>,
                         request<key<K>, monotone>,
                         request<key<K>, sorted, monotone>,
                         request<key<K>, contiguous, frozen>,
                         request<key<K>, sorted, contiguous, frozen>>;
using ordered_sets = requests<request<key<int>, sorted, order_by<negated>>, request<key<int>, sorted, frozen, order_by<negated>>>;

using indices = requests<
    request<key<index<Id>>, dense>,
    request<key<index<Id>>, value<int>, dense>,
    request<key<index<Id>>, value<int>, dense, constant_clear>,
    request<key<index<Id>>>,
    request<key<index<Id>>, sorted>,
    request<key<index<Id>>, value<int>, dense, layered>,
    request<key<index<Id>>, value<Store<value<Store<value<index<Id>>, frozen>>, handles, unique, monotone, layered>>, dense, layered>,
    request<key<index<Id>>, value<Store<value<Store<value<index<Id>>, frozen>>, handles, unique, monotone, layered, concurrent>>, dense, layered, concurrent>>;

template<typename I>
using dense_sets_of = requests<request<key<I>, dense>,
                               request<key<I>, dense, sorted>,
                               request<key<I>, dense, frozen>,
                               request<key<I>, dense, frozen, sorted>,
                               request<key<I>, dense, constant_clear>,
                               request<key<I>, dense, sorted, constant_clear>,
                               request<key<I>, dense, sorted, contiguous, frozen>,
                               request<key<I>, dense, proportional_clear>,
                               request<key<I>, dense, insertion_ordered>,
                               request<key<I>, dense, contiguous>,
                               request<key<I>, dense, monotone>,
                               request<key<I>, dense, small>,
                               request<key<I>, dense, concurrent>,
                               request<key<I>, dense, stable_refs>,
                               request<key<I>, dense, change_log>,
                               request<key<I>, partition>>;

template<typename K, typename V>
using maps_of = requests<request<key<K>, value<V>>,
                         request<key<K>, value<V>, sorted>,
                         request<key<K>, value<V>, small>,
                         request<key<K>, value<V>, frozen>,
                         request<key<K>, value<V>, sorted, frozen>,
                         request<key<K>, value<V>, multi>,
                         request<key<K>, value<V>, multi, stable_refs>,
                         request<key<K>, value<V>, multi, sorted>,
                         request<key<K>, value<V>, multi, frozen>,
                         request<key<K>, value<V>, stable_refs>,
                         request<key<K>, value<V>, retains>,
                         request<key<K>, value<V>, reclaims>,
                         request<key<K>, value<V>, insertion_ordered>,
                         request<key<K>, value<V>, scoped>,
                         request<key<K>, value<V>, sorted, scoped>,
                         request<key<K>, value<V>, concurrent, layered>,
                         request<key<K>, value<V>, layered>,
                         request<key<K>, value<V>, stable_refs, layered>,
                         request<key<K>, value<V>, concurrent>,
                         request<key<K>, value<V>, stable_refs, concurrent>,
                         request<key<K>, value<V>, concurrent, retains>,
                         request<key<K>, value<V>, monotone>,
                         request<key<K>, value<V>, sorted, monotone>,
                         request<key<K>, value<Store<value<V>, frozen>>>,
                         request<key<K>, value<Store<value<V>, retains>>>,
                         request<key<K>, value<Store<value<V>, retains>>, sorted>,
                         request<key<K>, value<Store<value<V>>>>,
                         request<key<K>, value<Store<key<V>>>>>;
using ordered_maps = requests<request<key<int>, value<int>, sorted, order_by<negated>>,
                              request<key<int>, value<int>, sorted, small, order_by<by_value>>,
                              request<key<int>, value<int>, sorted, small, multi, order_by<by_value>>,
                              request<key<int>, value<int>, sorted, small, stable_refs, order_by<by_value>>,
                              request<key<int>, value<int>, sorted, frozen, order_by<negated>>>;

template<typename I, typename V>
using dense_maps_of = requests<request<key<I>, value<V>, dense>,
                               request<key<I>, value<V>, dense, sorted>,
                               request<key<I>, value<V>, dense, frozen>,
                               request<key<I>, value<V>, dense, constant_clear>,
                               request<key<I>, value<V>, dense, contiguous>,
                               request<key<I>, value<V>, dense, constant_clear, concurrent>,
                               request<key<I>, value<V>, dense, insertion_ordered>,
                               request<key<I>, value<V>, dense, stable_refs>,
                               request<key<I>, value<V>, dense, constant_clear, stable_refs>,
                               request<key<I>, value<V>, dense, concurrent>,
                               request<key<I>, value<V>, dense, layered>,
                               request<key<I>, value<V>, dense, scoped>,
                               request<key<I>, value<V>, dense, constant_clear, layered>,
                               request<key<I>, value<V>, dense, monotone>,
                               request<key<I>, value<V>, multi, dense>,
                               request<key<I>, value<V>, multi, dense, frozen>,
                               request<key<I>, value<V>, multi, dense, frozen, sorted>,
                               request<key<I>, value<I>, multi, dense, frozen, bidirectional>,
                               request<key<I>, value<I>, multi, dense, bidirectional>,
                               request<key<I>, value<Store<key<I>, dense>>, dense>,
                               request<key<I>, value<Store<key<I>, dense, constant_clear>>, dense, constant_clear>,
                               request<key<I>, value<Store<value<V>, frozen>>, dense, frozen>>;

using sequences = requests<request<value<int>>,
                           request<value<int>, sorted>,
                           request<value<int>, frozen>,
                           request<value<int>, small>,
                           request<value<int>, nonempty>,
                           request<value<int>, retains>,
                           request<value<int>, reclaims>,
                           request<value<int>, contiguous>,
                           request<value<int>, monotone>,
                           request<value<int>, stable_refs, monotone>,
                           request<value<int>, stable_refs, frozen>,
                           request<value<int>, monotone, concurrent>,
                           request<value<std::string>>,
                           request<value<Id>, sorted>,
                           request<value<Store<value<int>>>>,
                           request<value<Store<value<int>, frozen>>>,
                           request<value<Store<value<int>, frozen>>, frozen>,
                           request<value<Store<value<Store<value<int>, frozen>>, frozen>>>,
                           request<value<Store<key<int>, dense, frozen>>>,
                           request<value<Store<value<int>, retains>>, retains>,
                           request<value<Store<value<int>>>, fixed_shape>,
                           request<value<Store<key<int>, dense>>, fixed_shape>,
                           request<value<Store<key<int>, dense>>, fixed_shape, proportional_clear>,
                           request<value<Store<value<Store<key<int>, dense>>, fixed_shape>>, fixed_shape>>;

template<typename T>
using queues_of = requests<request<value<T>, fifo>,
                           request<value<T>, lifo>,
                           request<value<T>, by_priority>,
                           request<value<T>, by_priority, handles>,
                           request<value<T>, fifo, small>,
                           request<value<T>, fifo, stable_refs>,
                           request<value<T>, fifo, retains>,
                           request<value<T>, lifo, constant_clear>,
                           request<value<T>, by_priority, constant_clear>,
                           request<value<T>, by_priority, monotone>>;
using ordered_queues = requests<request<value<int>, by_priority, monotone_priority>,
                                request<value<int>, by_priority, order_by<negated>>,
                                request<value<int>, by_priority, handles, order_by<negated>>,
                                request<value<int>, by_priority, monotone_priority, order_by<negated>>>;

using pools = requests<request<value<Store<value<int>, frozen>>, handles, unique>,
                       request<value<Store<key<int>, dense, frozen>>, handles, unique>,
                       request<value<Store<value<int>, frozen>>, handles, unique, structural_sharing>,
                       request<value<Store<key<int>, dense, frozen>>, handles, unique, structural_sharing>,
                       request<value<Store<value<int>, frozen>>, handles, unique, concurrent>,
                       request<value<Store<key<int>, dense, frozen>>, handles, unique, structural_sharing, concurrent>,
                       request<value<Store<value<int>, frozen>>, handles, unique, monotone>,
                       request<value<std::vector<int>>, handles>,
                       request<value<std::vector<int>>, handles, concurrent>,
                       request<value<int>, handles>,
                       request<value<Id>, handles, stable_refs>,
                       request<value<Store<value<int>, frozen>>, handles, unique, monotone, layered>,
                       request<value<Store<key<int>, dense, frozen>>, handles, unique, layered, concurrent>,
                       request<value<int>, handles, layered>,
                       request<value<Store<value<int>, frozen>>, handles, unique, structural_sharing, layered>>;

using stores_as_keys = requests<request<key<Store<key<int>, sorted, frozen>>>,
                                request<key<Store<key<int>, sorted, frozen>>, value<int>>,
                                request<key<Store<key<int>, dense, frozen, sorted>>>,
                                request<key<Store<value<int>, frozen>>>,
                                request<key<Store<key<int>, sorted>>, sorted>,
                                request<key<Store<key<int>>>>,
                                request<key<Store<key<int>, dense, frozen>>>>;

using sparse_partitions = requests<request<key<std::string>, partition>>;

using flat_pools = requests<request<value<Id>, handles, unique, flat>,
                            request<value<Id>, handles, unique, flat, monotone>,
                            request<value<Id>, handles, unique, flat, stable_refs>,
                            request<value<Id>, handles, unique, flat, concurrent>,
                            request<value<Text>, handles, unique, flat>,
                            request<value<Text>, handles, unique, flat, monotone, concurrent>,
                            request<value<Id>, handles, unique, flat, monotone, layered>,
                            request<value<Text>, handles, unique, flat, layered, concurrent>>;

/// A group of the table: its title beside its requests.
template<urd::detail::fixed_string Title, typename List>
struct group
{
    static constexpr const char* title = Title.c_str();
    using list = List;
};

/// The groups, in the order of the table.
using groups = std::tuple<group<"Sets of int", sets_of<int>>,
                          group<"Sets of std::string", sets_of<std::string>>,
                          group<"Sets of Id", sets_of<Id>>,
                          group<"Sets ordered by a projection", ordered_sets>,
                          group<"Dense sets of int", dense_sets_of<int>>,
                          group<"Dense sets of Id", dense_sets_of<Id>>,
                          group<"Keyed by an index", indices>,
                          group<"Maps int to int", maps_of<int, int>>,
                          group<"Maps std::string to double", maps_of<std::string, double>>,
                          group<"Maps Id to Id", maps_of<Id, Id>>,
                          group<"Maps ordered by a projection", ordered_maps>,
                          group<"Dense maps int to int", dense_maps_of<int, int>>,
                          group<"Dense maps Id to double", dense_maps_of<Id, double>>,
                          group<"Sequences", sequences>,
                          group<"Queues of int", queues_of<int>>,
                          group<"Queues of Id", queues_of<Id>>,
                          group<"Queues ordered by a projection", ordered_queues>,
                          group<"Pools", pools>,
                          group<"Stores as keys", stores_as_keys>,
                          group<"Partitions of sparse keys", sparse_partitions>,
                          group<"Records stored flat", flat_pools>>;

/// Calls f(title, requests<…> {}) for each group, in the order of the table.
template<typename F>
void for_each_group(F f)
{
    [&]<std::size_t... G>(std::index_sequence<G...>) { (f(std::tuple_element_t<G, groups>::title, typename std::tuple_element_t<G, groups>::list {}), ...); }(
        std::make_index_sequence<std::tuple_size_v<groups>>());
}

template<std::size_t P, typename List>
struct nth_request;
template<std::size_t P, typename... Rs>
struct nth_request<P, requests<Rs...>>
{
    using type = std::tuple_element_t<P, std::tuple<Rs...>>;
};

/// The candidate named Name that serves spec S: how a per-container object finds its container among a request's
/// candidates (one name may stand for several types, a layered pool of rows or of records). Reads declarations only.
template<urd::detail::fixed_string Name, typename S, typename List = typename urd::detail::candidates_for<S>::type>
struct serving_named;
template<urd::detail::fixed_string Name, typename S, typename... Rs>
struct serving_named<Name, S, urd::detail::type_list<Rs...>>
{
    static constexpr std::size_t at()
    {
        constexpr std::array<std::string_view, sizeof...(Rs)> ids { name<Rs>::id... };
        constexpr std::array<bool, sizeof...(Rs)> serves { urd::detail::serves<Rs, S>... };
        for (std::size_t each = 0; each < ids.size(); ++each)
            if (ids[each] == std::string_view(Name.c_str()) && serves[each])
                return each;
        return ids.size();
    }
    static_assert(at() < sizeof...(Rs), "serving_named: the container named serves no such request");
    using type = std::tuple_element_t<at(), std::tuple<Rs...>>;
};

/// The request at position P of group G: how a benchmark names the request it measures a container for.
template<std::size_t G, std::size_t P>
using request_at = typename nth_request<P, typename std::tuple_element_t<G, groups>::list>::type;

}  // namespace urd::test
