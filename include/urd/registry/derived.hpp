#pragma once

// What urd's traits say about a registered container, derived from its declaration (urd/registry/declaration.hpp)
// and written nowhere else: container_traits (its offer for the elements its template arguments give), element_traits
// and relies_on (from that offer), container_costs and container_synopsis (the declaration's), and defined_in (its own
// header, the last of its headers).

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
    declaration_of<R>::features;
};
template<typename R>
using declaration_t = typename declaration_of<R>::declaration;

/// R's offer for the elements its template arguments give, or nothing where it does not take them.
template<declared_container R>
inline constexpr offered offer_v = declaration_t<R>::offers(declaration_of<R>::features);

}  // namespace detail

template<detail::declared_container R>
struct container_traits<R>
{
    static constexpr bool declared = detail::offer_v<R>.has_value();
    static constexpr detail::property_set urd_properties = declared ? detail::offer_v<R>->properties : detail::property_set::roots();
};

template<detail::declared_container R>
    requires(detail::offer_v<R>.has_value())
struct element_traits<R>
{
    static constexpr detail::property_set urd_properties = detail::offer_v<R>->nested;
    static constexpr bool declared = true;
};

template<detail::declared_container R>
    requires(detail::offer_v<R>.has_value())
inline constexpr detail::property_set relies_on<R> = detail::offer_v<R>->relies_on;

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
