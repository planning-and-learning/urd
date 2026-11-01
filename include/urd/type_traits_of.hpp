#pragma once

// The type traits of elements and orders, each its words and what it means (holds), so that the registry only
// combines tags and traits (urd/registry/scan.hpp): what a type is (can_hash, an_index, …) and what an order is
// (own_order, natural_ordered, …); what rows allow is the row containers' (urd/repr/sequences/rows_fwd.hpp). From them,
// the roles of a request or a container's template arguments, as facts about roles (urd/property_set.hpp, tag::role).

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

}  // namespace urd::detail

namespace urd::trait
{

// What a type is, for the containers that hold it: derived from the types, never stated. Each trait is its words and
// what it means of a plain type (holds), read for its role by urd::detail::traits_holding (urd/registry/facts.hpp); a
// Store answers for itself (urd/store/construction.hpp, store_facts), so no trait is asked of one.

// What an element, a map's value or a row's element is (role element, mapped, row_element).
struct can_hash
{
    static constexpr std::string_view name = "can_hash";
    static constexpr std::string_view description = "urd::Hash and urd::EqualTo hash and compare it";
    template<typename T>
    static consteval bool holds()
    { return hashable<T>; }
};
struct an_index
{
    static constexpr std::string_view name = "an_index";
    static constexpr std::string_view description = "an index 0..n-1 (index_of)";
    template<typename T>
    static consteval bool holds()
    { return indexable<T>; }
};
struct copies_as_bytes
{
    static constexpr std::string_view name = "copies_as_bytes";
    static constexpr std::string_view description = "trivially copyable: copied as bytes";
    template<typename T>
    static consteval bool holds()
    { return std::is_trivially_copyable_v<T>; }
};
struct destroys_trivially
{
    static constexpr std::string_view name = "destroys_trivially";
    static constexpr std::string_view description = "trivially destructible: destroyed without code";
    template<typename T>
    static consteval bool holds()
    { return std::is_trivially_destructible_v<T>; }
};
struct names_members
{
    static constexpr std::string_view name = "names_members";
    static constexpr std::string_view description = "names its members for cista (cista_members)";
    template<typename T>
    static consteval bool holds()
    {
        return requires { T::cista_members(); };
    }
};
struct whole_value
{
    static constexpr std::string_view name = "whole_value";
    static constexpr std::string_view description = "a value interned whole: no Store and no container of elements, which a pool holds as rows";
    template<typename T>
    static consteval bool holds()
    { return !detail::held_as<T>::store; }
};

// What an order C over elements Ordered is (role order).
struct own_order
{
    static constexpr std::string_view name = "own_order";
    static constexpr std::string_view description = "ordered by urd::Less: no order_by";
    template<typename Ordered, typename C>
    static consteval bool holds()
    { return std::same_as<C, Less<Ordered>>; }
};
struct natural_ordered
{
    static constexpr std::string_view name = "natural_ordered";
    static constexpr std::string_view description = "ordered as sorted means";
    template<typename Ordered, typename C>
    static consteval bool holds()
    { return natural_order<C, Ordered>; }
};
struct integral_priority
{
    static constexpr std::string_view name = "integral_priority";
    static constexpr std::string_view description = "what the order compares is an integral number";
    template<typename Ordered, typename C>
    static consteval bool holds()
    {
        if constexpr (requires { typename detail::order_key_t<Ordered, C>; })
            return std::integral<detail::order_key_t<Ordered, C>>;
        else
            return false;
    }
};

}  // namespace urd::trait

template<>
struct urd::traits_of_role<urd::role::element>
{
    using type =
        detail::type_list<trait::can_hash, trait::an_index, trait::copies_as_bytes, trait::destroys_trivially, trait::names_members, trait::whole_value>;
};
template<>
struct urd::traits_of_role<urd::role::mapped> : traits_of_role<role::element>
{
};
template<>
struct urd::traits_of_role<urd::role::row_element> : traits_of_role<role::element>
{
};
template<>
struct urd::traits_of_role<urd::role::order>
{
    using type = detail::type_list<trait::own_order, trait::natural_ordered, trait::integral_priority>;
};

namespace urd::detail
{

/// A row's facts are its own tags (where it is a Store or a registered container); its elements' are their traits
/// (none where it is no row: its elements are void).
template<>
struct role_facts<role::row>
{
    template<typename T>
    static consteval property_set of()
    { return held_as<T>::properties; }
};
template<>
struct role_facts<role::row_element>
{
    template<typename T>
    static consteval property_set of()
    { return traits_holding<role::row_element, typename held_as<T>::elements>(); }
};

/// The facts by role of a kind's played types (role_types<plays<Role, Args…>…>): each role's facts, about that role.
template<typename Played>
consteval property_set facts_of()
{
    auto result = property_set::roots();
    []<typename... Plays>(role_types<Plays...> /*played*/, property_set& into)
    {
        const auto put = [&into]<typename Role, typename... Args>(plays<Role, Args...> /*plays*/)
        { about(role_v<Role>, role_facts<Role>::template of<Args...>()).for_each([&into](tag fact) { into.put(fact); }); };
        (put(Plays {}), ...);
    }(Played {}, result);
    return result;
}

/// The facts by role of Data, a kind with its element types (urd::set<K>, …) or a request's spec, ordered by Order:
/// what a container's template arguments make (declaration_of::roles) and what a request's data is.
template<typename Data, typename Order = Less<typename Data::ordered_type>>
consteval property_set roles_of()
{ return facts_of<typename Data::template played<Order>>(); }

/// The facts of spec S: what it promises, and its data's facts by role.
template<typename S>
consteval property_set request_roles()
{
    auto result = S::promised;
    roles_of<S, typename S::order_type>().for_each([&result](tag fact) { result.put(fact); });
    return result;
}

}  // namespace urd::detail
