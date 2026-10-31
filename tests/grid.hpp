#pragma once

// A grid of requests, each well-formed, over a few element types: what the resolution table (docs/RESOLUTION.md,
// written and checked by tests/resolution_table.cpp) and the registry audit (tests/registry.cpp) run over. Every
// representation pin in tests/store.cpp and tests/survey.cpp has its shape here. Nothing instantiates a Store: a request's spec and its resolution read only
// traits.

#include "names.hpp"
#include "urd/flat.hpp"
#include "urd/store.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cxxabi.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace urd::test
{

/// A key identified by one integral member: hashable, orderable and indexable.
struct Id
{
    std::uint32_t id = 0;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Id::id> {}; }
};

/// A record with text: serialized into the arena where stored flat.
struct Text
{
    urd::flat_string text;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Text::text> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Text::text> {}; }
};

/// Projections for order_by: by the negated element, and a map's entries by their values.
inline constexpr auto negated = [](int element) { return -element; };
inline constexpr auto by_value = [](const int& /*key*/, const int& value) { return value; };

namespace detail
{

inline std::string demangled(const char* mangled)
{
    int status = 0;
    const auto freed = std::unique_ptr<char, void (*)(void*)>(abi::__cxa_demangle(mangled, nullptr, nullptr, &status), std::free);
    return status == 0 ? std::string(freed.get()) : std::string(mangled);
}

}  // namespace detail

template<typename T>
std::string type_name();

/// A spec as a request: its key and value types, then its tags, in the order of urd::tag.
template<typename S>
std::string describe_spec()
{
    auto result = std::string("Store<");
    auto separator = std::string_view();
    const auto add = [&](const std::string& part)
    {
        result += separator;
        result += part;
        separator = ", ";
    };
    if constexpr (!std::is_void_v<typename S::key_type>)
        add("key<" + type_name<typename S::key_type>() + ">");
    if constexpr (!std::is_void_v<typename S::value_type>)
        add("value<" + type_name<typename S::value_type>() + ">");
    for (std::size_t each = 0; each < urd::detail::tag_count; ++each)
    {
        const auto root = static_cast<urd::tag>(each);
        if (urd::detail::root_of(root) == root && S::urd_properties.in(root) != root)
            add(std::string(tag_name(S::urd_properties.in(root))));
    }
    if constexpr (!std::is_void_v<typename S::order_type>)
        add("order_by<P>");
    return result + ">";
}

template<typename T>
std::string type_name()
{
    if constexpr (std::is_same_v<T, int>)
        return "int";
    else if constexpr (std::is_same_v<T, unsigned>)
        return "unsigned";
    else if constexpr (std::is_same_v<T, std::size_t>)
        return "std::size_t";
    else if constexpr (std::is_same_v<T, double>)
        return "double";
    else if constexpr (std::is_same_v<T, std::string>)
        return "std::string";
    else if constexpr (std::is_same_v<T, Id>)
        return "Id";
    else if constexpr (urd::is_index<T>)
        return "index<" + type_name<typename T::element_type>() + ">";
    else if constexpr (std::is_same_v<T, Text>)
        return "Text";
    else if constexpr (urd::is_store<T>)
        return describe_spec<urd::traits_t<T>>();
    else
        return detail::demangled(typeid(T).name());
}
template<typename T>
std::string type_name(std::type_identity<std::vector<T>>)
{ return "std::vector<" + type_name<T>() + ">"; }

/// The families whose headers a Store of R needs: R's, and its parts' (a wrapper's part comes first, as its error
/// does). Named by the header's stem: `hash`, `hash + layers`.
template<typename R>
void families_of(std::vector<std::string>& into)
{
    [&]<typename... Ps>(urd::detail::type_list<Ps...>) { (families_of<Ps>(into), ...); }(typename urd::parts<R>::type {});
    auto header = std::string(urd::defined_in<R>.c_str());
    header = header.substr(header.rfind('/') + 1);
    header = header.substr(0, header.rfind('.'));
    if (header == "<unnamed>")
        throw std::runtime_error("a registered representation names no family header (urd::defined_in, beside its declaration): "
                                 + std::string(urd::test::name<R>::id));
    if (std::find(into.begin(), into.end(), header) == into.end())
        into.push_back(header);
}
/// The families of the Stores a type names as its elements, nested ones too (a Store of Stores of ints), by matching
/// the Store's type, so none of them is completed.
template<typename T>
void element_families_of(std::vector<std::string>& into);
template<typename R, typename S>
void element_families_of_store(std::type_identity<urd::detail::Store<R, S>> /*store*/, std::vector<std::string>& into)
{
    families_of<R>(into);
    if constexpr (!std::is_void_v<typename S::key_type>)
        element_families_of<typename S::key_type>(into);
    if constexpr (!std::is_void_v<typename S::value_type>)
        element_families_of<typename S::value_type>(into);
}
template<typename T>
void element_families_of(std::vector<std::string>& into)
{
    if constexpr (urd::is_store<T>)
        urd::test::element_families_of_store(std::type_identity<T> {}, into);  // qualified: lookup by argument would complete the Store
}

/// One request: its spec, and how to print it as written.
template<typename... Ps>
struct request
{
    using spec = typename urd::detail::to_spec<Ps...>::type;

    template<typename P>
    static std::string one()
    {
        if constexpr (urd::detail::a_tag<P>)
            return std::string(tag_name(P::value));
        else if constexpr (urd::detail::is_a<urd::key, P>)
            return "key<" + type_name<typename P::type>() + ">";
        else if constexpr (urd::detail::is_a<urd::value, P>)
            return "value<" + type_name<typename P::type>() + ">";
        else
            return "order_by<P>";
    }
    static std::string describe()
    {
        auto result = std::string("Store<");
        auto separator = std::string_view();
        ((result += separator, result += one<Ps>(), separator = ", "), ...);
        return result + ">";
    }
};

template<typename... Rs>
struct requests
{
};
template<typename... Lists>
struct cat;
template<>
struct cat<>
{
    using type = requests<>;
};
template<typename... Rs>
struct cat<requests<Rs...>>
{
    using type = requests<Rs...>;
};
template<typename... As, typename... Bs, typename... Lists>
struct cat<requests<As...>, requests<Bs...>, Lists...> : cat<requests<As..., Bs...>, Lists...>
{
};
template<typename... Lists>
using cat_t = typename cat<Lists...>::type;

/// Calls f on each request type of a list (as std::type_identity<request<…>>).
template<typename F, typename... Rs>
void for_each_request(requests<Rs...> /*list*/, F f)
{ (f(std::type_identity<Rs> {}), ...); }

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

/// The groups, in the order of the table, and their titles.
using groups = std::tuple<sets_of<int>,
                          sets_of<std::string>,
                          sets_of<Id>,
                          ordered_sets,
                          dense_sets_of<int>,
                          dense_sets_of<Id>,
                          indices,
                          maps_of<int, int>,
                          maps_of<std::string, double>,
                          maps_of<Id, Id>,
                          ordered_maps,
                          dense_maps_of<int, int>,
                          dense_maps_of<Id, double>,
                          sequences,
                          queues_of<int>,
                          queues_of<Id>,
                          ordered_queues,
                          pools,
                          stores_as_keys,
                          sparse_partitions,
                          flat_pools>;
inline constexpr std::array<const char*, std::tuple_size_v<groups>> group_titles {
    "Sets of int",
    "Sets of std::string",
    "Sets of Id",
    "Sets ordered by a projection",
    "Dense sets of int",
    "Dense sets of Id",
    "Keyed by an index",
    "Maps int to int",
    "Maps std::string to double",
    "Maps Id to Id",
    "Maps ordered by a projection",
    "Dense maps int to int",
    "Dense maps Id to double",
    "Sequences",
    "Queues of int",
    "Queues of Id",
    "Queues ordered by a projection",
    "Pools",
    "Stores as keys",
    "Partitions of sparse keys",
    "Records stored flat",
};

/// Calls f(title, requests<…> {}) for each group, in the order of the table.
template<typename F>
void for_each_group(F f)
{
    [&]<std::size_t... G>(std::index_sequence<G...>)
    { (f(group_titles[G], std::tuple_element_t<G, groups> {}), ...); }(std::make_index_sequence<std::tuple_size_v<groups>>());
}

template<std::size_t P, typename List>
struct nth_request;
template<std::size_t P, typename... Rs>
struct nth_request<P, requests<Rs...>>
{
    using type = std::tuple_element_t<P, std::tuple<Rs...>>;
};

/// The request at position P of group G: how a benchmark names the request it measures a container for.
template<std::size_t G, std::size_t P>
using request_at = typename nth_request<P, std::tuple_element_t<G, groups>>::type;

}  // namespace urd::test
