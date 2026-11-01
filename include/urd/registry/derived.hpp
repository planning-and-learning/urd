#pragma once

// What urd's traits say about a registered container, derived from its declaration (urd/registry/declaration.hpp)
// and written nowhere else: container_traits (what it provides, where its template arguments give elements it takes),
// relies_on (what a request must state for it), container_costs and container_synopsis (the declaration's), and
// defined_in (its own header, the last of its headers).

#include "urd/costs.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/traits.hpp"

#include <type_traits>

namespace urd
{

/// What representation R is: `id`, a short stable name (`unordered_flat_set`); `description`, what it is
/// (`boost::unordered_flat_set (open-addressing hash table)`); and `holds`, what it keeps besides its elements, where
/// its memory goes. Declared for every registered representation; none here.
template<typename R>
struct container_synopsis
{
};

namespace detail
{

/// Whether R is a registered container: one with a declaration.
template<typename R>
concept declared_container = requires {
    typename declaration_of<R>::declaration;
    declaration_of<R>::roles;
};
template<typename R>
using declaration_t = typename declaration_of<R>::declaration;

/// Whether R takes the elements its template arguments give: what they are meets what its declaration relies on (its
/// elements', values' and rows' tags; what a Store must state is a request's to promise, relies_on below).
template<declared_container R>
inline constexpr bool takes_v = [] { return promises(declaration_of<R>::roles, facts_where(declaration_t<R>::relies_on, true)); }();

}  // namespace detail

template<detail::declared_container R>
struct container_traits<R>
{
    static constexpr detail::property_set urd_properties = detail::takes_v<R> ? detail::declaration_t<R>::provides : detail::property_set::roots();
};

template<detail::declared_container R>
struct container_costs<R> : detail::declaration_t<R>::operation_costs
{
};

template<detail::declared_container R>
struct container_synopsis<R>
{
    static constexpr std::string_view id = detail::declaration_t<R>::id;
    static constexpr std::string_view description = detail::declaration_t<R>::description;
    static constexpr std::string_view holds = detail::declaration_t<R>::holds;
};

template<detail::declared_container R>
inline constexpr auto defined_in<R> = detail::fixed_string<detail::declaration_t<R>::headers.back().size() + 1>(detail::declaration_t<R>::headers.back());

}  // namespace urd
