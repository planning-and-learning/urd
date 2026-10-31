#pragma once

// A map in insertion order over an insertion-ordered set of keys, declared with what the registry reads of it.
// Defined in urd/repr/ordered_map.hpp, which the family of its keys includes (urd/use/dense.hpp for a sparse set,
// urd/use/hash.hpp for an indexed set).

#include "urd/costs.hpp"
#include "urd/synopsis.hpp"
#include "urd/traits.hpp"

#include <type_traits>

namespace urd::repr
{

/// A map in insertion order: its keys in Keys, an insertion-ordered set whose positions index the values, and the
/// values in a vector beside them.
template<typename Keys, typename V>
class OrderedMap;

}  // namespace urd::repr

namespace urd
{

/// In the order of its keys, which are as Keys keeps them (dense or not). Its entries are no contiguous pairs. Clear
/// costs what clearing Keys does where values need no destructor, else at most time in proportion to the entries.
template<typename Keys, typename V>
    requires has<container_traits<Keys>, insertion_ordered, unique>
struct container_traits<repr::OrderedMap<Keys, V>> :
    with_t<container_traits<Keys>,
           functional,
           by_key,  // the keys are positional, the entries are not: a map is reached by key
           any_layout,
           std::conditional_t<detail::trivially_destructible<V> && has<container_traits<Keys>, constant_clear>,
                              constant_clear,
                              std::conditional_t<has<container_traits<Keys>, proportional_clear>, proportional_clear, any_clear>>>
{
};

/// Defined where its keys are: the map adds nothing a family of its own would hold.
template<typename Keys, typename V>
inline constexpr auto defined_in<repr::OrderedMap<Keys, V>> = defined_in<Keys>;

}  // namespace urd

namespace urd
{

/// ordered_map: its part's costs, but for what it declares here.
template<typename Keys, typename V>
struct container_costs<repr::OrderedMap<Keys, V>> : part_costs<Keys>
{
    static constexpr cost erase = costs::lin_n;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = part_costs<Keys>::words + 1;
};

}  // namespace urd

namespace urd
{

template<typename Keys, typename V>
struct container_synopsis<repr::OrderedMap<Keys, V>>
{
    static constexpr std::string_view id = "ordered_map";
    static constexpr std::string_view description = "ordered map (keys in an insertion-ordered set, values beside them)";
    static constexpr std::string_view holds = "its key set + n values in order";
};

}  // namespace urd
