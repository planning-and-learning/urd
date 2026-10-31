#pragma once

// Layers: a set, map or pool that answers lookups from its own elements, then from a parent's it neither owns nor
// changes. Declared with what the registry reads of them; defined in urd/repr/layered.hpp, which urd/use/layers.hpp
// includes. What is layered comes with its own family.

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
inline constexpr auto defined_in<repr::Layered<R>> = detail::fixed_string("urd/use/layers.hpp");
template<typename R>
inline constexpr auto defined_in<repr::LayeredPool<R>> = detail::fixed_string("urd/use/layers.hpp");

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

template<typename R>
struct container_synopsis<repr::LayeredPool<R>>
{
    static constexpr std::string_view id = "layered_pool";
    static constexpr std::string_view description = "layered pool (its own elements after a parent's, indices continuing)";
    static constexpr std::string_view holds = "its own rows, indices after the parent's";
};

template<typename R>
struct container_synopsis<repr::Layered<R>>
{
    static constexpr std::string_view id = "layered";
    static constexpr std::string_view description = "layered (its own elements, then a parent's)";
    static constexpr std::string_view holds = "its own elements; the parent is not owned";
};

template<typename T>
struct container_synopsis<repr::Layered<std::unordered_set<T, Hash<T>, EqualTo<T>>>>
{
    static constexpr std::string_view id = "layered_node_set";
    static constexpr std::string_view description = "layered node set (std::unordered_set over a parent's)";
    static constexpr std::string_view holds = "its own elements in nodes; the parent is not owned";
};

template<typename K, typename V>
struct container_synopsis<repr::Layered<std::unordered_map<K, V, Hash<K>, EqualTo<K>>>>
{
    static constexpr std::string_view id = "layered_node_map";
    static constexpr std::string_view description = "layered node map (std::unordered_map over a parent's)";
    static constexpr std::string_view holds = "its own entries in nodes; the parent is not owned";
};

template<typename K, typename V>
struct container_synopsis<repr::Layered<repr::IndexMap<K, V, true>>>
{
    static constexpr std::string_view id = "layered_generation_map";
    static constexpr std::string_view description = "layered generation map (a slot per index, O(1) clear, over a parent's)";
    static constexpr std::string_view holds = "its own slots; the parent is not owned";
};

}  // namespace urd
