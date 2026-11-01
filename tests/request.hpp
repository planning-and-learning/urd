#pragma once

// Requests as types, for the tests and benchmarks that walk many of them (a table of what each resolves to, a benchmark
// per container): a request is the parameter pack a Store is written with, its spec what it resolves
// from, and lists of requests join and are walked without instantiating a Store. Which headers a Store needs is read
// from declarations alone (urd::defined_in, urd::parts), so walking requests completes nothing.

#include "urd/format.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/store.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace urd
{

/// One request: the parameters a Store is written with (Store<Ps...>), and the spec they resolve from.
template<typename... Ps>
struct request
{
    using spec = typename detail::to_spec<Ps...>::type;
    using stated = detail::type_list<Ps...>;
};

/// A list of requests.
template<typename... Rs>
using requests = detail::type_list<Rs...>;
/// Lists of requests, joined in order.
template<typename... Lists>
using cat_t = typename detail::join<Lists...>::type;

/// Calls f on each request type of a list, as std::type_identity<request<...>>.
template<typename F, typename... Rs>
void for_each_request(requests<Rs...> /*list*/, F f)
{ (f(std::type_identity<Rs> {}), ...); }

template<typename T>
void element_headers_of(std::vector<std::string>& into);

namespace detail
{

template<typename R, typename S>
void element_headers_of_store(std::type_identity<detail::Store<R, S>> /*store*/, std::vector<std::string>& into)
{
    // From the row the Store's request resolves to, which lists them whether or not they are included here.
    for (const auto header : detail::choose<S>::headers)
        if (!header.empty() && std::ranges::find(into, header) == into.end())
            into.emplace_back(header);
    [&into]<typename... Types>(type_list<Types...> /*types*/)
    { (element_headers_of<Types>(into), ...); }(typename argument_types<typename S::arguments>::type {});
}

}  // namespace detail

/// The headers of the Stores a type names as its elements, nested ones too (a Store of Stores of ints), by matching
/// the Store's type, so none of them is completed.
template<typename T>
void element_headers_of(std::vector<std::string>& into)
{
    if constexpr (is_store<T>)
        detail::element_headers_of_store(std::type_identity<T> {}, into);  // qualified: lookup by argument would complete the Store
}

namespace detail
{

template<typename P>
std::string spelling_of_stated()
{
    if constexpr (a_tag<P>)
        return std::string(tag_name(P::value));
    else
        return spelling_of_argument<P>();
}

template<typename List>
struct stated_spelling;
template<typename... Ps>
struct stated_spelling<type_list<Ps...>>
{
    static std::string spelled() { return fmt::format("Store<{}>", fmt::join(std::array<std::string, sizeof...(Ps)> { spelling_of_stated<Ps>()... }, ", ")); }
};

}  // namespace detail

/// Request Q (urd::request) as it is written: Store<key<int>, sorted>, its parameters in the order stated.
template<typename Q>
std::string spelling_of_request()
{ return detail::stated_spelling<typename Q::stated>::spelled(); }

}  // namespace urd
