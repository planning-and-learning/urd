#pragma once

// The element features of a request (urd/registry/row.hpp), computed once per request from its element, key and
// mapped types and its order_by comparator: everything any container's guarantees depend on, so that the rows can be
// evaluated as values. Each feature is read as the constraint of the container it stands for is: where a trait does
// not apply to a type, the feature is simply false, as a constraint is simply unsatisfied.

#include "urd/elements.hpp"
#include "urd/registry/element_concepts.hpp"
#include "urd/registry/row.hpp"
#include "urd/registry/rows.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <string_view>
#include <type_traits>
#include <vector>

namespace urd::detail
{

template<typename T>
consteval type_features features_of_type();

/// The facts about T, once per type; a Store's elements' facts are reached through it.
template<typename T>
inline constexpr type_features features_v = features_of_type<T>();

template<typename T>
consteval type_features features_of_type()
{
    auto result = type_features {};
    if constexpr (!std::is_void_v<T>)
    {
        result.hashable = hashable<T>;
        result.orderable = orderable<T>;
        result.indexable = indexable<T>;
        result.integral = std::integral<T>;
        result.trivially_copyable = trivially_copyable<T>;
        result.trivially_destructible = trivially_destructible<T>;
        result.default_initializable = slot_value<T>;
        if constexpr (!is_store<T>)  // asking a Store for a member would complete it
            result.serializable = requires { T::cista_members(); };
        else
        {
            using traits = container_traits<T>;
            result.store = true;
            result.store_properties = traits::urd_properties;
            result.store_required = traits::required;
            result.store_kind = traits::kind;
        }
        if constexpr (requires { typename row_element_t<T>; })
            result.element = &features_v<row_element_t<T>>;
    }
    return result;
}

/// The element features of spec S: what every row reads.
template<typename S>
consteval request_features features_of()
{
    using P = typename S::order_type;
    /// What an order orders: a map's keys, else the elements.
    using ordered = std::conditional_t<S::is_map, typename S::key_type, typename S::element_type>;
    auto result = request_features {};
    if constexpr (!S::is_map)
        result.element = features_v<typename S::element_type>;
    result.key = features_v<typename S::key_type>;
    result.mapped = features_v<typename S::mapped_type>;
    if constexpr (is_entry_less<P>)
        result.by = order::entry;
    else
    {
        using compare = std::conditional_t<std::is_void_v<P>, Less<ordered>, P>;
        if constexpr (!std::is_void_v<P>)
        {
            result.by = order::element;
            result.order_natural = natural_order<P, ordered>;
        }
        if constexpr (requires { typename repr::bucket_priority_t<ordered, compare>; })
            result.order_key = features_v<repr::bucket_priority_t<ordered, compare>>;
    }
    return result;
}

}  // namespace urd::detail

namespace urd::detail
{

/// The row of the container spec S resolves to, chosen among the rows as values (urd/registry/rows.hpp), or none:
/// what choose<S> (urd/registry/select.hpp) resolves by.
template<typename S>
consteval const registry_row* row_choice()
{
    constexpr auto f = features_of<S>();
    return first_serving_row(registry_table, request_shape { S::kind, S::urd_properties, S::required }, f);
}

}  // namespace urd::detail
