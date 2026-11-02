#pragma once

#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/less.hpp"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace urd
{

// What a container needs of its elements to keep its properties. A container declares a property only for
// elements that allow it: a tree is sorted only for orderable keys. urd::Hash, urd::EqualTo and urd::Less
// (include/urd/elements/) cover numbers, strings and the standard's composites, and any type that names its
// identity with identifying_members(); nothing has to go into namespace std.

/// Ordered by urd::Less and compared by urd::EqualTo: what sorted means.
template<typename T>
concept orderable = OrderedByLess<T> && EqualityComparableByEqualTo<T>;

/// Hashed by urd::Hash and compared by urd::EqualTo: what a hash table needs to find an element.
template<typename T>
concept hashable = Hashable<T> && EqualityComparableByEqualTo<T>;

namespace detail
{

/// What order Compare compares elements T by: its projection's result where it projects (urd::by<P>), else T.
template<typename T, typename Compare>
struct order_key_of
{
    using type = T;
};
template<typename T, typename Compare>
    requires requires { Compare::projection; }
struct order_key_of<T, Compare>
{
    using type = std::remove_cvref_t<std::invoke_result_t<decltype(Compare::projection), const T&>>;
};
template<typename T, typename Compare>
using order_key_t = typename order_key_of<T, Compare>::type;

}  // namespace detail

/// Compares by P(element) under urd::Less, and nothing else: two elements P does not separate are equivalent (in a
/// unique Store, one element). P is a member pointer or a stateless callable; a request orders by it as by any other
/// comparator: order_by<by<&Job::deadline>>.
template<auto P>
struct by
{
    static constexpr auto projection = P;

    template<typename T>
    constexpr bool operator()(const T& lhs, const T& rhs) const
    {
        return Less<detail::order_key_t<T, by>> {}(std::invoke(P, lhs), std::invoke(P, rhs));
    }
};

/// Comparator C orders T as sorted means: urd::Less over an orderable T, or a comparator a request gives
/// (order_by<C>) that is a strict weak order over T, which the request vouches for.
template<typename C, typename T>
concept natural_order = (std::same_as<C, Less<T>> && orderable<T>) || (!std::same_as<C, Less<T>> && std::strict_weak_order<C, const T&, const T&>);

namespace detail
{

/// The single member of a member-identified type, as Members<&K::m> names it.
template<typename List>
struct single_member
{
};
template<auto M>
struct single_member<Members<M>>
{
    static constexpr auto pointer = M;
};

template<typename K>
concept single_member_identified = MemberIdentified<K> && requires { single_member<decltype(K::identifying_members())>::pointer; };

}  // namespace detail

/// How a key is an index 0..n-1, for containers that store by position (dense keys): to_index, from_index, and
/// is_index for keys that are none (a negative integer), which lookups report absent.
/// Given for integers (which must not be negative), enums, and types identified by one integral member, the
/// shape of an index wrapper; specialize it for others.
template<typename K>
struct index_of;

template<std::integral K>
struct index_of<K>
{
    /// Whether key is an index at all; a lookup of one that is not finds nothing.
    [[nodiscard]] static constexpr bool is_index(K key) noexcept
    {
        if constexpr (std::is_signed_v<K>)
            return key >= 0;
        else
            return true;
    }
    [[nodiscard]] static constexpr std::size_t to_index(K key) noexcept
    {
        assert(is_index(key) && "urd::index_of: a negative key is no index");
        return static_cast<std::size_t>(key);
    }
    [[nodiscard]] static constexpr K from_index(std::size_t index) noexcept { return static_cast<K>(index); }
};

template<Enumeration K>
struct index_of<K>
{
    using underlying = std::underlying_type_t<K>;

    [[nodiscard]] static constexpr bool is_index(K key) noexcept { return index_of<underlying>::is_index(static_cast<underlying>(key)); }
    [[nodiscard]] static constexpr std::size_t to_index(K key) noexcept { return index_of<underlying>::to_index(static_cast<underlying>(key)); }
    [[nodiscard]] static constexpr K from_index(std::size_t index) noexcept { return static_cast<K>(index_of<underlying>::from_index(index)); }
};

template<detail::single_member_identified K>
    requires std::integral<std::remove_cvref_t<decltype(std::declval<const K&>().*detail::single_member<decltype(K::identifying_members())>::pointer)>>
struct index_of<K>
{
    static constexpr auto member = detail::single_member<decltype(K::identifying_members())>::pointer;
    using value_type = std::remove_cvref_t<decltype(std::declval<const K&>().*member)>;

    [[nodiscard]] static constexpr bool is_index(const K& key) noexcept { return index_of<value_type>::is_index(key.*member); }
    [[nodiscard]] static constexpr std::size_t to_index(const K& key) noexcept { return index_of<value_type>::to_index(key.*member); }
    [[nodiscard]] static constexpr K from_index(std::size_t index) noexcept
    {
        auto key = K {};
        key.*member = index_of<value_type>::from_index(index);
        return key;
    }
};

/// A key that is an index: what dense containers need of their keys.
template<typename K>
concept indexable = requires(const K& key, std::size_t index) {
    { index_of<K>::to_index(key) } -> std::same_as<std::size_t>;
    { index_of<K>::from_index(index) } -> std::same_as<K>;
    { index_of<K>::is_index(key) } -> std::same_as<bool>;
};

}  // namespace urd
