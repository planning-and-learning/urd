#pragma once

// The element features of a request (urd/registry/facts.hpp), computed once per request from its element, key and
// mapped types and its order_by comparator: everything any container's guarantees depend on, so that the rows can be
// evaluated as values. Each feature is read as the constraint of the container it stands for is: where a trait does
// not apply to a type, the feature is simply false, as a constraint is simply unsatisfied.

#include "urd/elements.hpp"
#include "urd/registry/element_concepts.hpp"
#include "urd/registry/facts.hpp"
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

/// The facts an order gives: with C the comparator over `Ordered` (urd::Less<Ordered> where none is given), none, an
/// order over the elements (whether C orders them as sorted means, and what it compares), or one over entries.
template<typename Ordered, typename C>
consteval void add_order(request_features& f)
{
    if constexpr (is_entry_less<C>)
        f.by = order::entry;
    else
    {
        if constexpr (!std::same_as<C, Less<Ordered>>)
        {
            f.by = order::element;
            f.order_natural = natural_order<C, Ordered>;
        }
        if constexpr (requires { typename repr::bucket_priority_t<Ordered, C>; })
            f.order_key = features_v<repr::bucket_priority_t<Ordered, C>>;
    }
}

/// The facts of elements T in order C: what a set, sequence, queue or pool's declaration reads.
template<typename T, typename C = Less<T>>
consteval request_features element_facts()
{
    auto result = request_features {};
    result.element = features_v<T>;
    result.key = features_v<T>;
    add_order<T, C>(result);
    return result;
}
/// The facts of a map from K to V in order C.
template<typename K, typename V, typename C = Less<K>>
consteval request_features map_facts()
{
    auto result = request_features {};
    result.key = features_v<K>;
    result.mapped = features_v<V>;
    add_order<K, C>(result);
    return result;
}
/// The facts of elements T hashed by H and compared by E.
template<typename T, typename H, typename E>
consteval request_features hashed_element_facts()
{
    auto result = element_facts<T>();
    result.hash_natural = natural_hash<H, E, T>;
    return result;
}
template<typename K, typename V, typename H, typename E>
consteval request_features hashed_map_facts()
{
    auto result = map_facts<K, V>();
    result.hash_natural = natural_hash<H, E, K>;
    return result;
}

/// The element features of spec S: what every declaration reads.
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
    add_order<ordered, std::conditional_t<std::is_void_v<P>, Less<ordered>, P>>(result);
    return result;
}

}  // namespace urd::detail
