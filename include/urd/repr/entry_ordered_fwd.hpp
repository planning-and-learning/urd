#pragma once

// A map whose entries are kept in an order that may read the values, declared with what the registry reads of it.
// Defined in urd/repr/entry_ordered.hpp, which urd/use/contiguous.hpp includes.

#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/synopsis.hpp"
#include "urd/traits.hpp"

namespace urd::repr
{

/// One vector of entries in Order (urd::EntryLess: by P(key, value), ties by key), found by key by a scan.
template<typename K, typename V, typename Order>
class FlatEntryMap;

}  // namespace urd::repr

namespace urd
{

/// In the order the request gave, in one array; erase and clear keep it.
template<orderable K, typename V, typename Order>
    requires detail::is_entry_less<Order>
struct container_traits<repr::FlatEntryMap<K, V, Order>> : properties_of<sorted, unique, functional, positional, contiguous, shrinkable, retains>
{
};

template<typename K, typename V, typename Order>
inline constexpr bool orders_by_value<repr::FlatEntryMap<K, V, Order>> = true;

template<typename K, typename V, typename Order>
inline constexpr auto defined_in<repr::FlatEntryMap<K, V, Order>> = detail::fixed_string("urd/use/contiguous.hpp");

}  // namespace urd

namespace urd
{

/// flat_entry_map.
template<typename K, typename V, typename Order>
struct container_costs<repr::FlatEntryMap<K, V, Order>>
{
    static constexpr cost insert = costs::lin_n;
    static constexpr cost lookup = costs::lin_n;
    static constexpr cost erase = costs::lin_n;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 1.5;
};

}  // namespace urd

namespace urd
{

template<typename K, typename V, typename Order>
struct container_synopsis<repr::FlatEntryMap<K, V, Order>>
{
    static constexpr std::string_view id = "flat_entry_map";
    static constexpr std::string_view description = "flat entry map (entries in an order that reads their values, found by key)";
    static constexpr std::string_view holds = "n entries in one array; for few";
};

}  // namespace urd
