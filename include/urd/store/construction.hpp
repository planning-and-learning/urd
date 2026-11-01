#pragma once

// How a Store is constructed, as its request decides: whether it has a default constructor (a nonempty Store has
// none), whether it is built from a shape (fixed_shape rows), whether what it was built from must hold an element
// (nonempty), and whether its contents fix the order its elements come in, so that it hashes and orders by them
// (sorted, canonical, insertion_ordered, a sequence). The Store's constructors and its Hash and Less read these
// (store.hpp); a constructor cannot be a facet's, as only the Store initializes its representation.

#include "urd/store/types.hpp"

namespace urd::detail
{

template<typename S>
struct construction
{
    /// Whether what it holds after construction must be an element at least (a tag says so: nonempty), and so whether
    /// it may start empty.
    static constexpr bool needs_an_element = S::urd_properties.template any_says<&category_table::needs_an_element>();
    static constexpr bool default_constructible = !needs_an_element;
    /// Whether its contents fix the order its elements come in: hashed by it (sorted, canonical, a sequence).
    static constexpr bool hashed_in_order = S::iterable && S::urd_properties.template any_says<&category_table::fixes_hash_order>();
    /// Whether it is iterated in an order the request fixes: ordered by it (sorted, insertion_ordered, a sequence).
    static constexpr bool ordered_in_order = store_types<void, S>::ordered;
};

/// What a Store is as an element (a key, a map's value, a row's element), from its request alone, so it is never
/// completed to answer: it hashes where its contents fix their order and its elements hash; it is no index, no bytes,
/// no record.
template<typename T>
    requires is_store<T>
struct store_facts<role::element, T>
{
    static constexpr property_set value = []
    {
        using S = container_traits<T>;
        auto result = property_set::roots();
        put_trait<trait::can_hash>(result,
                                   construction<S>::hashed_in_order
                                       && traits_holding<role::element, typename S::element_type>().has(trait_tag<trait::can_hash>));
        return result;
    }();
};
template<typename T>
    requires is_store<T>
struct store_facts<role::mapped, T> : store_facts<role::element, T>
{
};
template<typename T>
    requires is_store<T>
struct store_facts<role::row_element, T> : store_facts<role::element, T>
{
};

/// What an order C over Stores is: by urd::Less, they order by their contents where those come in an order their
/// request fixes and their elements order; a given comparator orders them as it would any type.
template<typename T, typename C>
    requires is_store<T>
struct store_facts<role::order, T, C>
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
                                              construction<S>::ordered_in_order
                                                  && traits_holding<role::order, E, Less<E>>().has(trait_tag<trait::natural_ordered>));
        }
        else
            put_trait<trait::natural_ordered>(result, natural_order<C, T>);
        return result;
    }();
};

}  // namespace urd::detail
