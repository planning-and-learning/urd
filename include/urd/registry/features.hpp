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

/// The features of T, with P the request's order_by comparator (void where it gives none).
template<typename T, typename P>
consteval type_features features_of_type()
{
    auto result = type_features {};
    if constexpr (!std::is_void_v<T>)
    {
        result.present = true;
        result.hashable = hashable<T>;
        result.orderable = orderable<T>;
        if constexpr (!std::is_void_v<P>)
            result.ordered_by_request = natural_order<P, T>;
        result.indexable = indexable<T>;
        result.trivially_copyable = trivially_copyable<T>;
        result.trivially_destructible = trivially_destructible<T>;
        result.slot_value = slot_value<T>;
        result.flat_record = repr::flat_record<T>;
        result.integral_priority = requires { requires std::integral<repr::bucket_priority_t<T, Less<T>>>; };
        if constexpr (!std::is_void_v<P> && !is_entry_less<P>)  // an entry order projects entries, not elements
            result.integral_priority_by_request = requires { requires std::integral<repr::bucket_priority_t<T, P>>; };
        if constexpr (is_store<T>)
        {
            using traits = container_traits<T>;
            result.store = true;
            result.store_properties = traits::urd_properties;
            result.store_kind = traits::kind;
            result.store_requirement = traits::required;
            result.store_of_stores = is_store<typename traits::element_type>;
            result.flattenable = flattenable<T>;
            result.fixed_rows = fixed_rows<T>;
        }
        result.row_vector_hashable = requires { requires hashable<std::vector<row_element_t<T>>>; };
        result.row_element_hashable = requires { requires hashable<row_element_t<T>>; };
        result.row_element_indexable = requires { requires indexable<row_element_t<T>>; };
    }
    return result;
}

/// The element features of spec S: what every row reads.
template<typename S>
consteval request_features features_of()
{
    using P = typename S::order_type;
    auto result = request_features {};
    if constexpr (!S::is_map)
        result.element = features_of_type<typename S::element_type, P>();
    result.key = features_of_type<typename S::key_type, P>();
    result.mapped = features_of_type<typename S::mapped_type, void>();
    if constexpr (is_entry_less<P>)
        result.by = order::entry;
    else if constexpr (!std::is_void_v<P>)
        result.by = order::element;
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
