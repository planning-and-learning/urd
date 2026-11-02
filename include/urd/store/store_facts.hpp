#pragma once

// What a Store is as an element, an order's operand or a row's element, from its request alone (store_types): the
// facts the registry reads of a Store held in another, so that it is never completed to answer.

#include "urd/store/types.hpp"

namespace urd::detail
{

/// What a Store is as an element (a key; a map's value and a row's element share the element's traits), from its
/// request alone, so it is never completed to answer: it hashes where its contents fix their order and its elements
/// hash; it is no index, no bytes, no record.
template<typename T>
    requires is_store<T>
struct store_facts<traits_of_role<role::element>::type, T>
{
    static constexpr property_set value = []
    {
        using S = container_traits<T>;
        auto result = property_set::roots();
        put_trait<trait::can_hash>(result,
                                   store_types<S>::hashed_in_order
                                       && role_facts<role::element>::of<typename S::element_type>().has(trait_tag<trait::can_hash>));
        return result;
    }();
};
/// What an order C over Stores is: by urd::Less, they order by their contents where those come in an order their
/// request fixes and their elements order; a given comparator orders them as it would any type.
template<typename T, typename C>
    requires is_store<T>
struct store_facts<traits_of_role<role::order>::type, T, C>
{
    static constexpr property_set value = []
    {
        auto result = property_set::roots();
        if constexpr (std::same_as<C, Less<T>>)
        {
            using S = container_traits<T>;
            using E = typename S::element_type;
            result.put(trait_tag<trait::own_order>);
            put_trait<trait::natural_ordered>(result,
                                              store_types<S>::ordered && role_facts<role::order>::of<E, Less<E>>().has(trait_tag<trait::natural_ordered>));
        }
        else
            put_trait<trait::natural_ordered>(result, natural_order<C, T>);
        return result;
    }();
};

}  // namespace urd::detail
