#pragma once

// A map whose entries are kept in an order that may read the values, declared with what the registry reads of it.
// Defined in urd/repr/entry_ordered.hpp.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/traits.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/registry/guarantees.hpp"

namespace urd::repr
{

/// One vector of entries in Order (urd::EntryLess: by P(key, value), ties by key), found by key by a scan.
template<typename K, typename V, typename Order>
class FlatEntryMap;

}  // namespace urd::repr

namespace urd
{


template<typename K, typename V, typename Order>
inline constexpr bool orders_by_value<repr::FlatEntryMap<K, V, Order>> = true;


}  // namespace urd

namespace urd
{


}  // namespace urd

namespace urd
{

namespace detail
{

/// flat entry map (entries in an order that reads their values, found by key)
struct flat_entry_map_decl
{
    static constexpr std::string_view id = "flat_entry_map";
    static constexpr auto headers = header_list<1> { "urd/repr/entry_ordered.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(entry_orders, f.by))
            return {};
        const auto held = guarantees::flat_entry_map(f.key);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::lin_n;
        static constexpr cost lookup = costs::lin_n;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "flat entry map (entries in an order that reads their values, found by key)";
    static constexpr std::string_view holds = "n entries in one array; for few";
};
template<typename K, typename V, typename Order>
struct declaration_of<repr::FlatEntryMap<K, V, Order>>
{
    using declaration = flat_entry_map_decl;
    static constexpr request_features features = map_facts<K, V, Order>();
};

}  // namespace detail

}  // namespace urd
