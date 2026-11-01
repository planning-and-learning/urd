#pragma once

// Lists of types, as urd's machinery walks them: the kinds, a kind's declarations, the requests a tool walks.

#include <cstddef>
#include <tuple>

namespace urd::detail
{

/// A list of types.
template<typename... Ts>
struct type_list
{
};

/// Lists of types, joined in order.
template<typename... Lists>
struct join
{
    using type = type_list<>;
};
template<typename... Ts>
struct join<type_list<Ts...>>
{
    using type = type_list<Ts...>;
};
template<typename... As, typename... Bs, typename... Lists>
struct join<type_list<As...>, type_list<Bs...>, Lists...> : join<type_list<As..., Bs...>, Lists...>
{
};

/// Lists of one list template (type_list, or any L<Ts...>), joined in order into the first.
template<typename First, typename... Lists>
struct concat
{
    using type = First;
};
template<template<typename...> class L, typename... As, typename... Bs, typename... Rest>
struct concat<L<As...>, L<Bs...>, Rest...> : concat<L<As..., Bs...>, Rest...>
{
};

/// The last type of First, Ts....
template<typename First, typename... Ts>
struct last
{
    using type = std::tuple_element_t<sizeof...(Ts), std::tuple<First, Ts...>>;
};

/// The I-th type of a list.
template<std::size_t I, typename List>
struct nth;
template<std::size_t I, typename... Ts>
struct nth<I, type_list<Ts...>>
{
    using type = std::tuple_element_t<I, std::tuple<Ts...>>;
};

/// Whether T is Template<…>.
template<template<typename> class Template, typename T>
inline constexpr bool is_a = false;
template<template<typename> class Template, typename T>
inline constexpr bool is_a<Template, Template<T>> = true;

}  // namespace urd::detail
