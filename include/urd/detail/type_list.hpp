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

/// The I-th type of a list.
template<std::size_t I, typename List>
struct nth;
template<std::size_t I, typename... Ts>
struct nth<I, type_list<Ts...>>
{
    using type = std::tuple_element_t<I, std::tuple<Ts...>>;
};

}  // namespace urd::detail
