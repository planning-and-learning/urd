#pragma once

// A grid of requests, each well-formed, over a few element types: what the resolution table (docs/RESOLUTION.md,
// written and checked by tests/resolution_table.cpp) runs over. Every request the libraries use has its shape here.
// Nothing instantiates a Store: a request's spec and its resolution read only traits.

#include "elements.hpp"
#include "names.hpp"
#include "request.hpp"
#include "urd/elements/flat.hpp"
#include "urd/format.hpp"
#include "urd/registry/select.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <array>
#include <boost/mp11.hpp>
#include <concepts>
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

/// A comparator for order_by: by the negated element.
inline constexpr auto negated = [](int element) { return -element; };
using by_negated = urd::by<negated>;

// Requests as types, and the headers a Store needs, come from tests/request.hpp; a request is written as
// urd::spelling_of_request says (urd/format.hpp).
using urd::cat_t;
using urd::element_headers_of;
using urd::for_each_request;
using urd::requests;

/// One request of the grid: urd::request, with how it is written.
template<typename... Ps>
struct request : urd::request<Ps...>
{
    static std::string describe() { return urd::spelling_of_request<urd::request<Ps...>>(); }
};

// The groups. Unservable requests are in: the table shows "none" for them, which is what they are today: requests no
// container serves yet.

template<typename K>
using sets_of = requests<request<set<K>>,
                         request<set<K>, sorted>,
                         request<set<K>, sorted, frozen>,
                         request<set<K>, frozen>,
                         request<set<K>, small>,
                         request<set<K>, stable_refs>,
                         request<set<K>, small, stable_refs>,
                         request<set<K>, sorted, stable_refs>,
                         request<set<K>, small, sorted, stable_refs>,
                         request<set<K>, insertion_ordered>,
                         request<set<K>, positional>,
                         request<set<K>, positional, sorted>,
                         request<set<K>, positional, monotone>,
                         request<set<K>, positional, monotone, stable_refs>,
                         request<set<K>, positional, monotone, concurrent>,
                         request<set<K>, concurrent>,
                         request<set<K>, stable_refs, concurrent>,
                         request<set<K>, retains>,
                         request<set<K>, sorted, retains>,
                         request<set<K>, stable_refs, retains>,
                         request<set<K>, sorted, stable_refs, retains>,
                         request<set<K>, reclaims>,
                         request<set<K>, sorted, reclaims>,
                         request<set<K>, insertion_ordered, reclaims>,
                         request<set<K>, scoped>,
                         request<set<K>, positional, monotone, layered>,
                         request<set<K>, nonempty>,
                         request<set<K>, monotone>,
                         request<set<K>, monotone_until_clear>,
                         request<set<K>, sorted, monotone>,
                         request<set<K>, contiguous, frozen>,
                         request<set<K>, sorted, contiguous, frozen>>;
using ordered_sets = requests<request<set<int>, sorted, order_by<by_negated>>, request<set<int>, sorted, frozen, order_by<by_negated>>>;

using indices = requests<request<set<index<Id>>, dense>,
                         request<map<index<Id>, int>, dense>,
                         request<map<index<Id>, int>, dense, constant_clear>,
                         request<set<index<Id>>>,
                         request<set<index<Id>>, sorted>,
                         request<map<index<Id>, int>, dense, layered>,
                         request<map<index<Id>, Store<pool<Store<sequence<index<Id>>, frozen>>, monotone, layered>>, dense, layered>,
                         request<map<index<Id>, Store<pool<Store<sequence<index<Id>>, frozen>>, monotone, layered, concurrent>>, dense, layered, concurrent>,
                         request<pool<Store<sequence<index<Id>>, frozen>>, monotone, bit, layered>,
                         request<pool<Store<sequence<index<Id>>, frozen>>, monotone, bit, layered, concurrent>>;

template<typename I>
using dense_sets_of = requests<request<set<I>, dense>,
                               request<set<I>, dense, sorted>,
                               request<set<I>, dense, frozen>,
                               request<set<I>, dense, frozen, sorted>,
                               request<set<I>, dense, constant_clear>,
                               request<set<I>, dense, sorted, constant_clear>,
                               request<set<I>, dense, sorted, contiguous, frozen>,
                               request<set<I>, dense, proportional_clear>,
                               request<set<I>, dense, insertion_ordered>,
                               request<set<I>, dense, contiguous>,
                               request<set<I>, dense, monotone>,
                               request<set<I>, dense, small>,
                               request<set<I>, dense, concurrent>,
                               request<set<I>, dense, stable_refs>,
                               request<partition<I>>>;

template<typename K, typename V>
using maps_of = requests<request<map<K, V>>,
                         request<map<K, V>, sorted>,
                         request<map<K, V>, small>,
                         request<map<K, V>, frozen>,
                         request<map<K, V>, sorted, frozen>,
                         request<map<K, V>, multi>,
                         request<map<K, V>, multi, stable_refs>,
                         request<map<K, V>, multi, sorted>,
                         request<map<K, V>, multi, frozen>,
                         request<map<K, V>, stable_refs>,
                         request<map<K, V>, retains>,
                         request<map<K, V>, reclaims>,
                         request<map<K, V>, insertion_ordered>,
                         request<map<K, V>, scoped>,
                         request<map<K, V>, sorted, scoped>,
                         request<map<K, V>, concurrent, layered>,
                         request<map<K, V>, concurrent>,
                         request<map<K, V>, stable_refs, concurrent>,
                         request<map<K, V>, concurrent, retains>,
                         request<map<K, V>, monotone>,
                         request<map<K, V>, sorted, monotone>,
                         request<map<K, Store<sequence<V>, frozen>>>,
                         request<map<K, Store<sequence<V>, retains>>>,
                         request<map<K, Store<sequence<V>, retains>>, sorted>,
                         request<map<K, Store<sequence<V>>>>,
                         request<map<K, Store<set<V>>>>>;
using ordered_maps = requests<request<map<int, int>, sorted, order_by<by_negated>>, request<map<int, int>, sorted, frozen, order_by<by_negated>>>;

template<typename I, typename V>
using dense_maps_of = requests<request<map<I, V>, dense>,
                               request<map<I, V>, dense, sorted>,
                               request<map<I, V>, dense, frozen>,
                               request<map<I, V>, dense, constant_clear>,
                               request<map<I, V>, dense, contiguous>,
                               request<map<I, V>, dense, constant_clear, concurrent>,
                               request<map<I, V>, dense, insertion_ordered>,
                               request<map<I, V>, dense, stable_refs>,
                               request<map<I, V>, dense, constant_clear, stable_refs>,
                               request<map<I, V>, dense, concurrent>,
                               request<map<I, V>, dense, layered>,
                               request<map<I, V>, dense, scoped>,
                               request<map<I, V>, dense, constant_clear, layered>,
                               request<map<I, V>, dense, monotone>,
                               request<map<I, V>, multi, dense>,
                               request<map<I, V>, multi, dense, frozen>,
                               request<map<I, V>, multi, dense, frozen, sorted>,
                               request<map<I, I>, multi, dense, frozen, bidirectional>,
                               request<map<I, I>, multi, dense, bidirectional>,
                               request<map<I, Store<set<I>, dense>>, dense>,
                               request<map<I, Store<set<I>, dense, constant_clear>>, dense, constant_clear>,
                               request<map<I, Store<sequence<V>, frozen>>, dense, frozen>>;

using sequences = requests<request<sequence<int>>,
                           request<multiset<int>, sorted>,
                           request<sequence<int>, frozen>,
                           request<sequence<int>, small>,
                           request<sequence<int>, nonempty>,
                           request<sequence<int>, retains>,
                           request<sequence<int>, reclaims>,
                           request<sequence<int>, contiguous>,
                           request<sequence<int>, monotone>,
                           request<sequence<int>, monotone_until_clear>,
                           request<sequence<int>, stable_refs, monotone>,
                           request<sequence<int>, stable_refs, frozen>,
                           request<sequence<int>, monotone, concurrent>,
                           request<sequence<int>, stable_refs>,
                           request<sequence<int>, concurrent>,
                           request<sequence<std::string>>,
                           request<multiset<Id>, sorted>,
                           request<sequence<Store<sequence<int>>>>,
                           request<sequence<Store<sequence<int>, frozen>>>,
                           request<sequence<Store<sequence<int>, frozen>>, frozen>,
                           request<sequence<Store<sequence<Store<sequence<int>, frozen>>, frozen>>>,
                           request<sequence<Store<set<int>, dense, frozen>>>,
                           request<sequence<Store<sequence<int>, retains>>, retains>,
                           request<sequence<Store<sequence<int>>>, fixed_shape>,
                           request<sequence<Store<set<int>, dense>>, fixed_shape>,
                           request<sequence<Store<set<int>, dense>>, fixed_shape, proportional_clear>,
                           request<sequence<Store<sequence<Store<set<int>, dense>>, fixed_shape>>, fixed_shape>>;

template<typename T>
using queues_of = requests<request<queue<T>, fifo>,
                           request<queue<T>, lifo>,
                           request<queue<T>, by_priority>,
                           request<queue<T>, by_priority, handles>,
                           request<queue<T>, fifo, small>,
                           request<queue<T>, fifo, stable_refs>,
                           request<queue<T>, fifo, retains>,
                           request<queue<T>, lifo, constant_clear>,
                           request<queue<T>, by_priority, constant_clear>,
                           request<queue<T>, by_priority, monotone>>;
using ordered_queues = requests<request<queue<int>, by_priority, monotone_priority>,
                                request<queue<int>, by_priority, order_by<by_negated>>,
                                request<queue<int>, by_priority, handles, order_by<by_negated>>,
                                request<queue<int>, by_priority, monotone_priority, order_by<by_negated>>>;

using pools = requests<request<pool<Store<sequence<int>, frozen>>>,
                       request<pool<Store<set<int>, dense, frozen>>>,
                       request<pool<Store<sequence<int>, frozen>>, structural_sharing>,
                       request<pool<Store<set<int>, dense, frozen>>, structural_sharing>,
                       request<pool<Store<sequence<int>, frozen>>, concurrent>,
                       request<pool<Store<set<int>, dense, frozen>>, structural_sharing, concurrent>,
                       request<pool<Store<sequence<int>, frozen>>, monotone>,
                       request<slots<std::vector<int>>>,
                       request<slots<std::vector<int>>, concurrent>,
                       request<slots<int>>,
                       request<slots<Id>, stable_refs>,
                       request<pool<Store<sequence<int>, frozen>>, monotone, layered>,
                       request<pool<Store<sequence<int>, frozen>>, monotone_until_clear, retains, layered>,
                       request<pool<Store<set<int>, dense, frozen>>, layered, concurrent>,
                       request<slots<int>, layered>,
                       request<pool<Store<sequence<int>, frozen>>, structural_sharing, layered>>;

using stores_as_keys = requests<request<set<Store<set<int>, sorted, frozen>>>,
                                request<map<Store<set<int>, sorted, frozen>, int>>,
                                request<set<Store<set<int>, dense, frozen, sorted>>>,
                                request<set<Store<sequence<int>, frozen>>>,
                                request<set<Store<set<int>, sorted>>, sorted>,
                                request<set<Store<set<int>>>>,
                                request<set<Store<set<int>, dense, frozen>>>>;

using sparse_partitions = requests<request<partition<std::string>>>;

using flat_pools = requests<request<pool<Id>, flat>,
                            request<pool<Id>, flat, monotone>,
                            request<pool<Id>, flat, stable_refs>,
                            request<pool<Id>, flat, concurrent>,
                            request<pool<Text>, flat>,
                            request<pool<Text>, flat, monotone, concurrent>,
                            request<pool<Id>, flat, monotone, layered>,
                            request<pool<Text>, flat, layered, concurrent>,
                            request<pool<Id>>,
                            request<pool<Text>>,
                            request<pool<Text>, stable_refs>,
                            request<pool<std::string>>>;

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
                          group<"Records and values interned", flat_pools>>;

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

/// The container named Name that serves spec S: how a per-container object finds its container among those serving a
/// request. Its declaration is read without its header; its type is named where the header is included.
template<urd::detail::fixed_string Name, typename S>
struct serving_named
{
    static constexpr std::size_t at()
    {
        constexpr auto serving = urd::detail::serving_v<S>;
        for (std::size_t each = 0; each < serving.count; ++each)
            if (urd::detail::table_of<typename S::kind>[serving.held[each]].id == std::string_view(Name.c_str()))
                return serving.held[each];
        return urd::detail::no_entry;
    }
    static_assert(at() != urd::detail::no_entry, "serving_named: the container named serves no such request");
    using declaration = boost::mp11::mp_at_c<urd::detail::declarations_t<typename S::kind>, at()>;
    using type = typename urd::detail::representation_of<declaration, S>::type;
};

/// The tag type of answer A: its place among its category's tags (urd::tags_of); a given comparison is order_by<Compare>,
/// keyed slots keyed<Key>.
template<typename Compare, typename Key, auto A>
struct tag_of
{
    using type = boost::mp11::mp_at_c<typename urd::tags_of<decltype(A)>::type, static_cast<std::size_t>(A)>;
};
template<typename Compare, typename Key>
struct tag_of<Compare, Key, urd::comparison::given>
{
    using type = urd::order_by<Compare>;
};
template<typename Compare, typename Key>
struct tag_of<Compare, Key, urd::keying::keyed>
{
    using type = urd::keyed<Key>;
};

/// The Store for spec S over container R (which must serve S): its request, written back from S, with over<R>.
template<typename S, typename R>
struct store_over_of;
template<typename Kind, typename Compare, typename Key, auto... As, typename R>
struct store_over_of<urd::spec<Kind, Compare, Key, As...>, R>
{
    using type = urd::Store<Kind, typename tag_of<Compare, Key, As>::type..., urd::detail::over<R>>;
    static_assert(std::same_as<typename urd::detail::to_spec<Kind, typename tag_of<Compare, Key, As>::type...>::type, urd::spec<Kind, Compare, Key, As...>>,
                  "store_over: the request written back from the spec resolves to another spec");
};
template<typename S, typename R>
using store_over = typename store_over_of<S, R>::type;

/// The containers serving spec S, in the order they are preferred, as types: for files that include every container's
/// header.
template<typename S, typename... Decls>
auto containers_of(boost::mp11::mp_list<Decls...> /*declarations*/) -> boost::mp11::mp_list<typename urd::detail::representation_of<Decls, S>::type...>;
template<typename S>
using serving_containers = decltype(urd::test::containers_of<S>(urd::detail::serving_declarations<S> {}));

/// Calls f(std::type_identity<Decl>) for each declaration serving spec S, in the order they are preferred.
template<typename S, typename F>
void for_each_serving(F f)
{
    [&]<typename... Decls>(boost::mp11::mp_list<Decls...>) { (f(std::type_identity<Decls> {}), ...); }(urd::detail::serving_declarations<S> {});
}

/// The request at position P of group G: how a benchmark names the request it measures a container for.
template<std::size_t G, std::size_t P>
using request_at = typename nth_request<P, typename std::tuple_element_t<G, groups>::list>::type;

}  // namespace urd::test
