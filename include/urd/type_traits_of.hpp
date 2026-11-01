#pragma once

// The type traits (urd/registry/facts.hpp: urd::trait), each derived here from the types, so
// that the registry only combines tags and traits (urd/registry/scan.hpp): what a type is (can_hash, an_index, …),
// what an order is (own_order, natural_ordered, …), what rows allow (rows_flatten, …), and from them the roles of a
// request or a container's template arguments (role_tags).

#include "urd/elements.hpp"
#include "urd/kinds.hpp"
#include "urd/property_set.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/facts.hpp"
#include "urd/semantics/less.hpp"
#include "urd/tags.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <ranges>
#include <type_traits>

namespace urd::detail
{

/// What T is held as an element: a Store (its tags, what it requires, its kind, its elements' type), a registered
/// container holding elements (the same, from its declaration, requiring nothing), or plain.
template<typename T>
struct held_as
{
    static constexpr bool store = false;
    static constexpr property_set properties = property_set::roots();
    static constexpr property_set required = property_set::roots();
    using kind = sequence_kind;
    using elements = void;
};
template<typename T>
    requires is_store<T>
struct held_as<T>
{
    static constexpr bool store = true;
    static constexpr property_set properties = container_traits<T>::urd_properties;
    static constexpr property_set required = container_traits<T>::required;
    using kind = typename container_traits<T>::kind;
    using elements = typename container_traits<T>::element_type;
};
template<typename T>
    requires(!is_store<T> && declared_container<T> && std::ranges::range<T>)
struct held_as<T>
{
    static constexpr bool store = takes_v<T>;
    static constexpr property_set properties = store ? declaration_t<T>::provides : property_set::roots();
    static constexpr property_set required = property_set::roots();
    using kind = typename declaration_t<T>::kind;
    using elements = std::conditional_t<store, std::ranges::range_value_t<T>, void>;
};

/// Whether rows T flatten (frozen, holding elements or rows kept in order), whether they change in place (sequences,
/// dense sets' bits, or fixed-shape rows of such rows at any depth), and whether they may shrink, so emptied ones
/// can be kept for reuse: what a jagged array and a pool of rows store.
template<typename T>
consteval bool rows_flatten()
{
    using row = held_as<T>;
    return row::store && row::properties.has(change::frozen) && (!held_as<typename row::elements>::store || !row::kind::keeps_by_value);
}
template<typename T>
consteval bool rows_in_place()
{
    using row = held_as<T>;
    if constexpr (!row::store)
        return false;
    else if constexpr (!held_as<typename row::elements>::store)
        return !row::properties.has(change::frozen)
               && (!row::kind::keeps_by_value
                   || (std::same_as<typename row::kind, set_kind> && row::properties.has(uniqueness::unique) && row::properties.has(keys::dense)));
    else
        return !row::properties.has(change::frozen) && row::properties.has(shape::fixed_shape) && rows_in_place<typename row::elements>();
}
template<typename T>
consteval bool rows_reused()
{
    using row = held_as<T>;
    return row::store && !row::properties.has(change::monotone) && !row::properties.has(emptiness::nonempty) && !row::properties.has(shape::fixed_shape);
}

/// Whether T names its members for cista; never asked of a Store, which asking would complete.
template<typename T>
consteval bool names_members()
{
    if constexpr (is_store<T>)
        return false;
    else
        return requires { T::cista_members(); };
}

/// What type T is.
template<typename T>
consteval trait_set type_traits_of()
{
    auto result = trait_set {};
    if constexpr (!std::is_void_v<T>)
    {
        result.put_if<trait::can_hash>(hashable<T>);
        result.put_if<trait::an_index>(indexable<T>);
        result.put_if<trait::copies_as_bytes>(trivially_copyable<T>);
        result.put_if<trait::destroys_trivially>(trivially_destructible<T>);
        result.put_if<trait::names_members>(names_members<T>());
        result.put_if<trait::whole_value>(!held_as<T>::store);
    }
    return result;
}
template<typename T>
inline constexpr trait_set type_traits_v = type_traits_of<T>();

/// What rows T allow.
template<typename T>
consteval trait_set row_traits_of()
{
    auto result = trait_set {};
    if constexpr (!std::is_void_v<T>)
    {
        result.put_if<trait::rows_flatten>(rows_flatten<T>());
        result.put_if<trait::rows_in_place>(rows_in_place<T>());
        result.put_if<trait::rows_reused>(rows_reused<T>());
    }
    return result;
}

/// The tags of T held as an element when it is a Store or a registered container (a sequence is positional); else
/// the roots. And what its elements are.
template<typename T>
consteval property_set row_tags_of()
{
    using row = held_as<T>;
    auto result = row::properties;
    if (row::store && row::kind::positional)
        result.put(position::positional);
    return result;
}
template<typename T>
consteval trait_set row_element_traits_of()
{
    if constexpr (held_as<T>::store)
        return type_traits_v<typename held_as<T>::elements>;
    else
        return trait_set {};
}

/// What order C is over elements `Ordered` (urd::Less<Ordered> where the request gives none): its own order, whether
/// it orders as sorted means, whether it compares integers.
template<typename Ordered, typename C>
consteval trait_set order_traits_of()
{
    auto result = trait_set {};
    if constexpr (std::same_as<C, Less<Ordered>>)
    {
        result.put(trait_v<trait::own_order>);
        result.put_if<trait::natural_ordered>(orderable<Ordered>);
    }
    else
        result.put_if<trait::natural_ordered>(natural_order<C, Ordered>);
    if constexpr (requires { typename order_key_t<Ordered, C>; })
        result.put_if<trait::integral_priority>(std::integral<order_key_t<Ordered, C>>);
    return result;
}

/// The roles of elements T in order C (a set's keys, a sequence's, queue's or pool's values), with what a Store
/// states.
template<typename T, typename C = Less<T>>
consteval role_tags element_roles(const property_set& stated_tags = property_set::roots())
{
    auto result = role_tags { .stated = stated_tags };
    result.at(role_v<role::element>).traits = type_traits_v<T>;
    result.at(role_v<role::order>).traits = order_traits_of<T, C>();
    result.at(role_v<role::row>).tags = row_tags_of<T>();
    result.at(role_v<role::rows>).traits = row_traits_of<T>();
    result.at(role_v<role::row_element>).traits = row_element_traits_of<T>();
    return result;
}
/// The roles of a map from K to V in order C (over keys, or over entries: is_entry_less).
template<typename K, typename V, typename C = Less<K>>
consteval role_tags map_roles(const property_set& stated_tags = property_set::roots())
{
    auto order = trait_set_of<trait::by_entry>();
    if constexpr (!is_entry_less<C>)
        order = order_traits_of<K, C>();
    auto result = element_roles<K>(stated_tags);
    result.at(role_v<role::order>).traits = order;
    result.at(role_v<role::mapped>).traits = type_traits_v<V>;
    return result;
}

}  // namespace urd::detail
