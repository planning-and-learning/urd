#pragma once

// What urd's traits say about a registered container, derived from its declaration (urd/registry/declaration.hpp)
// and written nowhere else: container_traits (what it provides, where its template arguments give elements it takes),
// relies_on (what a request must state for it), container_costs and container_synopsis (the declaration's), and
// defined_in (its own header, the last of its headers).

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/traits.hpp"

#include <type_traits>

namespace urd
{

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
inline constexpr bool takes_v = []
{
    auto data = declaration_t<R>::relies_on;
    data.stated = property_set::roots();
    return promises(declaration_of<R>::roles, data);
}();

}  // namespace detail

template<detail::declared_container R>
struct container_traits<R>
{
    static constexpr bool declared = detail::takes_v<R>;
    static constexpr detail::property_set urd_properties = declared ? detail::declaration_t<R>::provides : detail::property_set::roots();
};

template<detail::declared_container R>
    requires(detail::takes_v<R>)
inline constexpr detail::property_set relies_on<R> = detail::declaration_t<R>::relies_on.stated;

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
