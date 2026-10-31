#pragma once

// What the data tags mean (urd/tags.hpp, the tags a request never states), each derived here from the types beside
// the tags, so that the registry only combines tags (urd/registry/scan.hpp): what a type is (can_hash, an_index, …),
// what rows it holds (rows_flatten, …), what an order is (own_order, natural_ordered, …), and from them the roles of a
// request or a container's template arguments (role_tags).

#include "urd/elements.hpp"
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

/// `base` with every category `more` answers taken from `more`.
constexpr property_set merged(property_set base, const property_set& more)
{
    for (std::size_t each = 0; each < tag_count; ++each)
        if (more.held[each] != tag_at(each))
            base.held[each] = more.held[each];
    return base;
}

/// What T is held as an element: a Store (its tags, what it requires, its kind, its elements' type), a registered
/// container holding elements (the same, from its declaration, requiring nothing), or plain.
template<typename T>
struct held_as
{
    static constexpr bool store = false;
    static constexpr property_set properties = property_set::roots();
    static constexpr property_set required = property_set::roots();
    static constexpr urd::kind kind = urd::kind::sequence;
    using elements = void;
};
template<typename T>
    requires is_store<T>
struct held_as<T>
{
    static constexpr bool store = true;
    static constexpr property_set properties = container_traits<T>::urd_properties;
    static constexpr property_set required = container_traits<T>::required;
    static constexpr urd::kind kind = container_traits<T>::kind;
    using elements = typename container_traits<T>::element_type;
};
template<typename T>
    requires(!is_store<T> && declared_container<T> && std::ranges::range<T>)
struct held_as<T>
{
    static constexpr bool store = takes_v<T>;
    static constexpr property_set properties = store ? declaration_t<T>::provides : property_set::roots();
    static constexpr property_set required = property_set::roots();
    static constexpr urd::kind kind = declaration_t<T>::kind;
    using elements = std::conditional_t<store, std::ranges::range_value_t<T>, void>;
};

/// Whether rows T flatten (frozen, holding elements or rows kept in order), whether they change in place (sequences,
/// dense sets' bits, or fixed-shape rows of such rows at any depth), and whether they may shrink, so emptied ones
/// can be kept for reuse: what a jagged array and a pool of rows store.
template<typename T>
consteval bool rows_flatten()
{
    using row = held_as<T>;
    return row::store && row::properties.has(change::frozen) && (!held_as<typename row::elements>::store || !keeps_by_value(row::kind));
}
template<typename T>
consteval bool rows_in_place()
{
    using row = held_as<T>;
    if constexpr (!row::store)
        return false;
    else if (row::properties.has(change::frozen))
        return false;
    else if constexpr (!held_as<typename row::elements>::store)
        return !keeps_by_value(row::kind) || (row::kind == urd::kind::set && row::properties.has(uniqueness::unique) && row::properties.has(keys::dense));
    else
        return row::properties.has(shape::fixed_shape) && rows_in_place<typename row::elements>();
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

/// The data tags of T: what it is, and what rows it holds.
template<typename T>
consteval property_set data_tags_of()
{
    auto result = property_set::roots();
    if constexpr (!std::is_void_v<T>)
    {
        const auto put = [&result](bool holds, tag each)
        {
            if (holds)
                result.put(each);
        };
        put(hashable<T>, hashability::can_hash);
        put(orderable<T>, comparing::can_order);
        put(indexable<T>, indexing::an_index);
        put(trivially_copyable<T>, copying::copies_as_bytes);
        put(trivially_destructible<T>, destruction::destroys_trivially);
        put(slot_value<T>, refilling::refillable);
        put(names_members<T>(), member_naming::names_members);
        put(rows_flatten<T>(), row_flattening::rows_flatten);
        put(rows_in_place<T>(), row_updates::rows_in_place);
        put(rows_reused<T>(), row_reuse::rows_reused);
    }
    return result;
}
template<typename T>
inline constexpr property_set data_tags_v = data_tags_of<T>();

/// The tags of T held as an element when it is a Store or a registered container (a sequence is positional), and its
/// elements' data tags; else the roots.
template<typename T>
consteval property_set row_tags_of()
{
    using row = held_as<T>;
    auto result = row::properties;
    if (row::store && (row::kind == urd::kind::sequence || row::kind == urd::kind::multiset))
        result.put(position::positional);
    return result;
}
template<typename T>
consteval property_set row_element_tags_of()
{
    if constexpr (held_as<T>::store)
        return data_tags_v<typename held_as<T>::elements>;
    else
        return property_set::roots();
}

/// The tags order C gives elements `Ordered` (urd::Less<Ordered> where the request gives none): its own order, as
/// sorted means, comparing integers.
template<typename Ordered, typename C>
consteval property_set order_tags_of()
{
    auto result = property_set::roots();
    if constexpr (std::same_as<C, Less<Ordered>>)
    {
        result.put(order_source::own_order);
        if (orderable<Ordered>)
            result.put(natural_order_source::natural_ordered);
    }
    else if (natural_order<C, Ordered>)
        result.put(natural_order_source::natural_ordered);
    if constexpr (requires { typename order_key_t<Ordered, C>; })
    {
        if (std::integral<order_key_t<Ordered, C>>)
            result.put(priority_key::integral_priority);
    }
    return result;
}

/// The roles of elements T in order C (a set's keys, a sequence's, queue's or pool's values), hashed by urd's hash or
/// not, with what a Store states.
template<typename T, typename C = Less<T>>
consteval role_tags element_roles(const property_set& stated = property_set::roots(), bool hashed = true)
{
    auto element = merged(data_tags_v<T>, order_tags_of<T, C>());
    if (hashed)
        element.put(hash_source::urd_hashed);
    return { .stated = stated, .element = element, .row = row_tags_of<T>(), .row_element = row_element_tags_of<T>() };
}
/// Of elements T hashed by H and compared by E.
template<typename T, typename H, typename E>
consteval role_tags hashed_element_roles()
{ return element_roles<T>(property_set::roots(), natural_hash<H, E, T>); }

/// The roles of a map from K to V in order C (over keys, or over entries: is_entry_less), hashed or not.
template<typename K, typename V, typename C = Less<K>>
consteval role_tags map_roles(const property_set& stated = property_set::roots(), bool hashed = true)
{
    auto key = data_tags_v<K>;
    if constexpr (is_entry_less<C>)
        key.put(entry_order::by_entry);
    else
        key = merged(key, order_tags_of<K, C>());
    if (hashed)
        key.put(hash_source::urd_hashed);
    return { .stated = stated, .element = key, .mapped = data_tags_v<V>, .row = row_tags_of<K>(), .row_element = row_element_tags_of<K>() };
}
/// Of keys K hashed by H and compared by E.
template<typename K, typename V, typename H, typename E>
consteval role_tags hashed_map_roles()
{ return map_roles<K, V>(property_set::roots(), natural_hash<H, E, K>); }

}  // namespace urd::detail
