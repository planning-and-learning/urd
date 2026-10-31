#pragma once

// Layers: a set, map or pool that answers lookups from its own elements, then from a parent's it neither owns nor
// changes. Declared with what the registry reads of them; defined in urd/repr/layer.hpp and
// urd/repr/layered_pool.hpp. What is layered is defined in its own header.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
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
inline constexpr auto defined_in<repr::Layered<R>> = detail::fixed_string("urd/repr/layer.hpp");
template<typename R>
inline constexpr auto defined_in<repr::LayeredPool<R>> = detail::fixed_string("urd/repr/layered_pool.hpp");

}  // namespace urd

namespace urd
{

/// layered_pool: its part's costs, but for what it declares here.
template<typename R>
struct container_costs<repr::LayeredPool<R>> : part_costs<part_of<repr::LayeredPool<R>>>
{
    static constexpr double words = part_costs<part_of<repr::LayeredPool<R>>>::words;
};

/// layered: its part's costs, but for what it declares here.
template<typename R>
struct container_costs<repr::Layered<R>> : part_costs<part_of<repr::Layered<R>>>
{
    static constexpr double words = part_costs<part_of<repr::Layered<R>>>::words;
};

}  // namespace urd

namespace urd
{

namespace detail
{
inline constexpr std::string_view layered_prefix = "layered_";
inline constexpr std::string_view layered_pool_description = "layered pool (its own elements after a parent's, indices continuing) of a ";
inline constexpr std::string_view layered_description = "layered (its own elements, then a parent's) over a ";
}  // namespace detail

/// A layered pool: `layered_` and its part's id (`layered_record_interner`).
template<typename R>
struct container_synopsis<repr::LayeredPool<R>>
{
    static constexpr std::string_view id = joined<detail::layered_prefix, container_synopsis<R>::id>::value;
    static constexpr std::string_view description = joined<detail::layered_pool_description, container_synopsis<R>::description>::value;
    static constexpr std::string_view holds = "its own rows, indices after the parent's";
};

/// A layered set or map: `layered_` and its part's id (`layered_unordered_flat_map`).
template<typename R>
struct container_synopsis<repr::Layered<R>>
{
    static constexpr std::string_view id = joined<detail::layered_prefix, container_synopsis<R>::id>::value;
    static constexpr std::string_view description = joined<detail::layered_description, container_synopsis<R>::description>::value;
    static constexpr std::string_view holds = "its own elements; the parent is not owned";
};

}  // namespace urd
