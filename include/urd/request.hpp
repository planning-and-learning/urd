#pragma once

// Requests as types, for tools that walk many of them (a table of what each resolves to, a benchmark per container,
// a search over the vocabulary): a request is the parameter pack a Store is written with, its spec what it resolves
// from, and lists of requests join and are walked without instantiating a Store. Which family headers a Store needs is
// read from declarations alone (urd::defined_in, urd::parts), so walking requests completes nothing.

#include "urd/container_synopsis.hpp"
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
/// Lists of requests, joined in order.
template<typename... Lists>
using cat_t = typename cat<Lists...>::type;

/// Calls f on each request type of a list, as std::type_identity<request<...>>.
template<typename F, typename... Rs>
void for_each_request(requests<Rs...> /*list*/, F f)
{ (f(std::type_identity<Rs> {}), ...); }

/// The families whose headers a Store over representation R needs, by the header's stem (`hash`, `layers`): R's
/// parts' first (a wrapper's part comes first, as its error does), then R's own. Throws for a representation that
/// names no family (urd::defined_in, beside its declaration).
template<typename R>
void families_of(std::vector<std::string>& into)
{
    [&]<typename... Ps>(detail::type_list<Ps...>) { (families_of<Ps>(into), ...); }(typename parts<R>::type {});
    auto header = std::string(defined_in<R>.c_str());
    header = header.substr(header.rfind('/') + 1);
    header = header.substr(0, header.rfind('.'));
    if (header == "<unnamed>")
    {
        auto id = std::string("a representation");
        if constexpr (requires { container_synopsis<R>::id; })
            id = std::string(container_synopsis<R>::id);
        throw std::runtime_error(id + " names no family header (urd::defined_in, beside its declaration)");
    }
    if (std::ranges::find(into, header) == into.end())
        into.push_back(header);
}

template<typename T>
void element_families_of(std::vector<std::string>& into);

namespace detail
{

template<typename R, typename S>
void element_families_of_store(std::type_identity<detail::Store<R, S>> /*store*/, std::vector<std::string>& into)
{
    families_of<R>(into);
    if constexpr (!std::is_void_v<typename S::key_type>)
        element_families_of<typename S::key_type>(into);
    if constexpr (!std::is_void_v<typename S::value_type>)
        element_families_of<typename S::value_type>(into);
}

}  // namespace detail

/// The families of the Stores a type names as its elements, nested ones too (a Store of Stores of ints), by matching
/// the Store's type, so none of them is completed.
template<typename T>
void element_families_of(std::vector<std::string>& into)
{
    if constexpr (is_store<T>)
        detail::element_families_of_store(std::type_identity<T> {}, into);  // qualified: lookup by argument would complete the Store
}

}  // namespace urd
