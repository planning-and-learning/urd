#pragma once

#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/semantics/less.hpp"

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
// (include/urd/semantics/) cover numbers, strings and the standard's composites, and any type that names its
// identity with identifying_members(); nothing has to go into namespace std.

/// Ordered by urd::Less and compared by urd::EqualTo: what sorted means.
template<typename T>
concept orderable = OrderedByLess<T> && EqualityComparableByEqualTo<T>;

/// Hashed by urd::Hash and compared by urd::EqualTo: what a hash table needs to find an element.
template<typename T>
concept hashable = Hashable<T> && EqualityComparableByEqualTo<T>;

/// Comparator C orders T as sorted means: urd::Less, or std::less where T's operator< is what urd::Less uses.
template<typename C, typename T>
concept natural_order = orderable<T> && (std::same_as<C, Less<T>> || (std::same_as<C, std::less<T>> && std::totally_ordered<T>) );

/// H and E hash and compare T as a hash table needs: urd's, or the standard's where T has them.
template<typename H, typename E, typename T>
concept natural_hash = hashable<T>
                       && ((std::same_as<H, Hash<T>> && std::same_as<E, EqualTo<T>>)
                           || (std::same_as<H, std::hash<T>> && std::same_as<E, std::equal_to<T>> && std::invocable<const std::hash<T>&, const T&>) );

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

/// How a key is an index 0..n-1, for containers that store by position (dense keys): to_index and from_index.
/// Given for integers (which must not be negative), enums, and types identified by one integral member, the
/// shape of an index wrapper; specialize it for others.
template<typename K>
struct index_of;

template<std::integral K>
struct index_of<K>
{
    [[nodiscard]] static constexpr std::size_t to_index(K key) noexcept
    {
        assert(key >= 0 && "urd::index_of: a negative key is no index");
        return static_cast<std::size_t>(key);
    }
    [[nodiscard]] static constexpr K from_index(std::size_t index) noexcept { return static_cast<K>(index); }
};

template<Enumeration K>
struct index_of<K>
{
    [[nodiscard]] static constexpr std::size_t to_index(K key) noexcept
    { return index_of<std::underlying_type_t<K>>::to_index(static_cast<std::underlying_type_t<K>>(key)); }
    [[nodiscard]] static constexpr K from_index(std::size_t index) noexcept { return static_cast<K>(index_of<std::underlying_type_t<K>>::from_index(index)); }
};

template<detail::single_member_identified K>
    requires std::integral<std::remove_cvref_t<decltype(std::declval<const K&>().*detail::single_member<decltype(K::identifying_members())>::pointer)>>
struct index_of<K>
{
    static constexpr auto member = detail::single_member<decltype(K::identifying_members())>::pointer;
    using value_type = std::remove_cvref_t<decltype(std::declval<const K&>().*member)>;

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
};

}  // namespace urd
