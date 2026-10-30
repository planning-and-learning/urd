#pragma once

// Layers: a set, map or pool that answers lookups from its own elements, then from a parent's it neither owns nor
// changes. Declared with what the registry reads of them; defined in urd/repr/layered.hpp, which urd/use/layers.hpp
// includes. What is layered comes with its own family.

#include "urd/traits.hpp"

namespace urd::repr
{

/// A set or map with unique keys over a parent: holds, iterates and erases its own elements, looks up in both.
template<typename R>
class Layered;
/// A pool over a parent pool: its indices continue the parent's, a lookup answers from the chain.
template<typename R>
class LayeredPool;

}  // namespace urd::repr

namespace urd
{

/// Its own elements as R holds them, plus the parent's in lookups, with indices continuing after the parent's.
template<typename R>
    requires has<container_traits<R>, handles, unique>
struct container_traits<repr::LayeredPool<R>> : with_t<container_traits<R>, layered>
{
};
template<typename R>
struct parts<repr::LayeredPool<R>>
{
    using type = detail::type_list<R>;
};
template<typename R>
struct element_traits<repr::LayeredPool<R>> : element_traits<R>
{
};

/// Its own elements as R holds them, plus the parent's in lookups; in no order, since the parent's elements are
/// not iterated with its own (a dense map's key order holds for each level alone).
template<typename R>
    requires has<container_traits<R>, unique>
struct container_traits<repr::Layered<R>> : with_t<container_traits<R>, layered, any_order>
{
};
template<typename R>
struct parts<repr::Layered<R>>
{
    using type = detail::type_list<R>;
};

template<typename R>
inline constexpr auto defined_in<repr::Layered<R>> = detail::fixed_string("urd/use/layers.hpp");
template<typename R>
inline constexpr auto defined_in<repr::LayeredPool<R>> = detail::fixed_string("urd/use/layers.hpp");

}  // namespace urd
