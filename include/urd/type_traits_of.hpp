#pragma once

// The type traits of elements and orders, each its words and what it means (holds), so that the registry only
// combines tags and traits (urd/registry/scan.hpp): what a type is (can_hash, an_index, …) and what an order is
// (own_order, natural_ordered, …); what rows allow is the row containers' (urd/repr/sequences/rows_fwd.hpp). From them,
// the roles of a request or a container's template arguments, as facts about roles (urd/property_set.hpp, tag::role).

#include "urd/elements/concepts.hpp"
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

/// What T is held as an element: a row (`row`: a Store, with its tags, what it requires, its kind and its elements'
/// type; or a registered container holding elements, the same from its declaration, requiring nothing), or plain.
template<typename T>
struct held_as
{
    static constexpr bool row = false;
    static constexpr property_set properties = property_set::roots();
    static constexpr property_set required = property_set::roots();
    using kind = void;
    using elements = void;
};
template<typename T>
    requires is_store<T>
struct held_as<T>
{
    static constexpr bool row = true;
    static constexpr property_set properties = container_traits<T>::urd_properties;
    static constexpr property_set required = container_traits<T>::required;
    using kind = typename container_traits<T>::kind;
    using elements = typename container_traits<T>::element_type;
};
template<typename T>
    requires(!is_store<T> && declared_container<T> && std::ranges::range<T>)
struct held_as<T>
{
    static constexpr bool row = takes_v<T>;
    static constexpr property_set properties = row ? declaration_t<T>::provides : property_set::roots();
    static constexpr property_set required = property_set::roots();
    using kind = typename declaration_t<T>::kind;
    using elements = std::conditional_t<row, std::ranges::range_value_t<T>, void>;
};

}  // namespace urd::detail

namespace urd::trait
{

// What a type is, for the containers that hold it: derived from the types, never stated. Each trait is its words and
// what it means of a plain type (holds), read for its role by urd::detail::role_facts (urd/registry/facts.hpp); a
// Store answers for itself (urd/store/store_facts.hpp, store_facts), so no trait is asked of one.

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
    { return Serializable<T>; }
};
struct whole_value
{
    static constexpr std::string_view name = "whole_value";
    static constexpr std::string_view description = "a value interned whole: no Store and no container of elements, which a pool holds as rows";
    template<typename T>
    static consteval bool holds()
    { return !detail::held_as<T>::row; }
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
        boost::mp11::mp_list<trait::can_hash, trait::an_index, trait::copies_as_bytes, trait::destroys_trivially, trait::names_members, trait::whole_value>;
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
    using type = boost::mp11::mp_list<trait::own_order, trait::natural_ordered, trait::integral_priority>;
};

namespace urd::detail
{

/// A row's facts are its own tags (where it is a Store or a registered container), whatever is asked.
template<typename Asked>
struct role_facts<role::row, Asked>
{
    template<typename T>
    static consteval property_set of()
    { return held_as<T>::properties; }
};

/// The facts by role of Data, a kind with its element types (urd::set<K>, …) or a request's spec, ordered by Order:
/// each role its played types (Data::played<Order>: plays<Role, Args…>) have, about that role, of the traits Asked
/// asks. What a container's template arguments make (declaration_of::roles: every trait) and what a request's data is.
template<typename Data, typename Order = Less<typename Data::ordered_type>, typename Asked = every_trait>
consteval property_set roles_of()
{
    auto result = property_set::roots();
    const auto add = [&result]<typename Role, typename... Args>(plays<Role, Args...> /*plays*/)
    { result = with(result, about(role_v<Role>, role_facts<Role, Asked>::template of<Args...>())); };
    []<typename... Plays>(boost::mp11::mp_list<Plays...> /*played*/, const auto& each) { (each(Plays {}), ...); }(typename Data::template played<Order> {}, add);
    return result;
}

/// The facts of spec S: what it promises, and its data's facts by role, of the traits Asked asks.
template<typename S, typename Asked>
consteval property_set request_roles()
{ return with(S::promised, roles_of<S, typename S::order_type, Asked>()); }

}  // namespace urd::detail
