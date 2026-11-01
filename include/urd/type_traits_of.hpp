#pragma once

// The type traits (urd/registry/facts.hpp: urd::trait), each derived here from the types, so
// that the registry only combines tags and traits (urd/registry/scan.hpp): what a type is (can_hash, an_index, …),
// what an order is (own_order, natural_ordered, …), what rows allow (rows_flatten, …), and from them the roles of a
// request or a container's template arguments, as facts about roles (urd/property_set.hpp, tag::role).

#include "urd/elements/less.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/kinds.hpp"
#include "urd/property_set.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/facts.hpp"
#include "urd/tags.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <ranges>
#include <type_traits>

namespace urd::detail
{

/// The elements of a row type, read without completing it: a Store's by its request (where it iterates: a pool or
/// queue has no elements to hand out), else what iterating a range gives. Undefined otherwise, so a constraint
/// naming it is simply not satisfied.
template<typename Row>
struct row_element
{
};
template<typename Row>
    requires(!is_store<Row>) && std::ranges::range<Row>
struct row_element<Row>
{
    using type = std::ranges::range_value_t<Row>;
};
template<typename Row>
    requires(is_store<Row> && container_traits<Row>::iterable)
struct row_element<Row>
{
    using type = typename container_traits<Row>::element_type;
};
template<typename Row>
using row_element_t = typename row_element<Row>::type;

/// What a Store is built, hashed and ordered by (urd/store/construction.hpp), read when a request is resolved.
template<typename S>
struct construction;

/// What T is held as an element: a Store (its tags, what it requires, its kind, its elements' type), a registered
/// container holding elements (the same, from its declaration, requiring nothing), or plain.
template<typename T>
struct held_as
{
    static constexpr bool store = false;
    static constexpr property_set properties = property_set::roots();
    static constexpr property_set required = property_set::roots();
    using kind = void;
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
    if constexpr (!row::store)
        return false;
    else
        return row::properties.has(change::frozen) && (!held_as<typename row::elements>::store || !by_value<T>);
}
template<typename T>
consteval bool rows_in_place()
{
    using row = held_as<T>;
    if constexpr (!row::store)
        return false;
    else if constexpr (!held_as<typename row::elements>::store)
        return !row::properties.has(change::frozen) && row::kind::changes_in_place(row::properties);
    else
        return !row::properties.has(change::frozen) && row::properties.has(shape::fixed_shape) && rows_in_place<typename row::elements>();
}
template<typename T>
consteval bool rows_reused()
{
    using row = held_as<T>;
    return row::store && !row::properties.has(change::monotone) && !row::properties.has(emptiness::nonempty) && !row::properties.has(shape::fixed_shape);
}

/// Whether T names its members for cista.
template<typename T>
consteval bool names_members()
{
    return requires { T::cista_members(); };
}

/// What type T is.
template<typename T>
consteval property_set type_traits_of()
{
    auto result = property_set::roots();
    if constexpr (is_store<T>)
    {
        // A Store is asked by its request, never completed (its container's header may be absent where it is named):
        // it owns storage, and hashes where its contents fix their order and its elements hash.
        using S = container_traits<T>;
        put_trait<trait::can_hash>(result, construction<S>::hashed_in_order && type_traits_of<typename S::element_type>().has(trait_tag<trait::can_hash>));
    }
    else if constexpr (!std::is_void_v<T>)
    {
        put_trait<trait::can_hash>(result, hashable<T>);
        put_trait<trait::an_index>(result, indexable<T>);
        put_trait<trait::copies_as_bytes>(result, std::is_trivially_copyable_v<T>);
        put_trait<trait::destroys_trivially>(result, std::is_trivially_destructible_v<T>);
        put_trait<trait::names_members>(result, names_members<T>());
        put_trait<trait::whole_value>(result, !held_as<T>::store);
    }
    return result;
}
template<typename T>
inline constexpr property_set type_traits_v = type_traits_of<T>();

/// What rows T allow.
template<typename T>
consteval property_set row_traits_of()
{
    auto result = property_set::roots();
    if constexpr (!std::is_void_v<T>)
    {
        put_trait<trait::rows_flatten>(result, rows_flatten<T>());
        put_trait<trait::rows_in_place>(result, rows_in_place<T>());
        put_trait<trait::rows_reused>(result, rows_reused<T>());
    }
    return result;
}

/// The tags of T held as an element when it is a Store or a registered container (a sequence is positional); else
/// the roots. And what its elements are.
template<typename T>
consteval property_set row_tags_of()
{
    using row = held_as<T>;
    return row::properties;
}
template<typename T>
consteval property_set row_element_traits_of()
{
    if constexpr (held_as<T>::store)
        return type_traits_v<typename held_as<T>::elements>;
    else
        return property_set::roots();
}

/// What order C is over elements `Ordered` (urd::Less<Ordered> where the request gives none): its own order, whether
/// it orders as sorted means, whether it compares integers.
template<typename Ordered, typename C>
consteval property_set order_traits_of()
{
    if constexpr (is_entry_less<C>)  // a map ordered by its entries: what is ordered is no one type
        return trait_set_of<trait::by_entry>();
    else if constexpr (std::same_as<C, Less<Ordered>> && is_store<Ordered>)
    {
        // A Store is asked by its request, never completed: it orders by its contents where they come in an order its
        // request fixes and its elements order.
        using S = container_traits<Ordered>;
        using E = typename S::element_type;
        auto result = trait_set_of<trait::own_order>();
        put_trait<trait::natural_ordered>(result, construction<S>::ordered_in_order && order_traits_of<E, Less<E>>().has(trait_tag<trait::natural_ordered>));
        return result;
    }
    else
    {
        auto result = property_set::roots();
        if constexpr (std::same_as<C, Less<Ordered>>)
        {
            result.put(trait_tag<trait::own_order>);
            put_trait<trait::natural_ordered>(result, orderable<Ordered>);
        }
        else
            put_trait<trait::natural_ordered>(result, natural_order<C, Ordered>);
        if constexpr (requires { typename order_key_t<Ordered, C>; })
            put_trait<trait::integral_priority>(result, std::integral<order_key_t<Ordered, C>>);
        return result;
    }
}

/// The roles of elements T in order C (a set's keys, a sequence's, queue's or pool's values), with what a Store
/// states.
template<typename T, typename C = Less<T>>
consteval property_set element_roles(const property_set& stated_tags = property_set::roots())
{
    auto result = stated_tags;
    for (const auto& each : { about(role_v<role::element>, type_traits_v<T>),
                              about(role_v<role::order>, order_traits_of<T, C>()),
                              about(role_v<role::row>, row_tags_of<T>()),
                              about(role_v<role::rows>, row_traits_of<T>()),
                              about(role_v<role::row_element>, row_element_traits_of<T>()) })
        each.for_each([&result](tag fact) { result.put(fact); });
    return result;
}
/// The roles of a map from K to V in order C (over keys, or over entries: is_entry_less).
template<typename K, typename V, typename C = Less<K>>
consteval property_set map_roles(const property_set& stated_tags = property_set::roots())
{
    auto result = element_roles<K, C>(stated_tags);
    about(role_v<role::mapped>, type_traits_v<V>).for_each([&result](tag fact) { result.put(fact); });
    return result;
}

/// The comparator spec S orders by: its order_by, else urd::Less over what its kind's data orders.
template<typename S>
using spec_order_t = std::conditional_t<std::is_void_v<typename S::order_type>, Less<typename S::ordered_type>, typename S::order_type>;

/// The roles of spec S, whatever its kind: what it promises; the traits of each type its data says plays a role (its
/// elements, a map's values); what its order is; and what the Stores it holds (held_type) are as rows.
template<typename S>
consteval property_set request_roles()
{
    auto result = S::promised;
    const auto put_all = [&result](const property_set& facts) { facts.for_each([&result](tag fact) { result.put(fact); }); };
    [&put_all]<typename... Plays>(role_types<Plays...> /*played*/)
    { (put_all(about(role_v<typename Plays::role>, type_traits_v<typename Plays::type>)), ...); }(typename S::played {});
    put_all(about(role_v<role::order>, order_traits_of<typename S::ordered_type, spec_order_t<S>>()));
    using held = typename S::held_type;
    put_all(about(role_v<role::row>, row_tags_of<held>()));
    put_all(about(role_v<role::rows>, row_traits_of<held>()));
    put_all(about(role_v<role::row_element>, row_element_traits_of<held>()));
    return result;
}

}  // namespace urd::detail
