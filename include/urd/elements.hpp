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

/// What order_by<P> means: T ordered by P(T) under urd::Less, ties by T itself where T is ordered. P is a member
/// pointer or a stateless callable.
template<typename T, auto P>
struct ProjectedLess
{
    static constexpr auto projection = P;

    constexpr bool operator()(const T& lhs, const T& rhs) const
    {
        using Projected = std::remove_cvref_t<std::invoke_result_t<decltype(P), const T&>>;
        // Projected twice rather than held: a member is projected for free, and holding a reference to what a
        // callable returns could outlive it.
        if (Less<Projected> {}(std::invoke(P, lhs), std::invoke(P, rhs)))
            return true;
        if (Less<Projected> {}(std::invoke(P, rhs), std::invoke(P, lhs)))
            return false;
        if constexpr (OrderedByLess<T>)
            return Less<T> {}(lhs, rhs);
        else
            return false;
    }
};

/// What order_by<P> means for a map whose P takes the key and the value: entries ordered by P(key, value) under
/// urd::Less, ties by the key, which identifies an entry. Compares any pair-like entries (std::pair<const K, V>, or
/// std::pair<K, V> as a flat representation keeps them).
template<typename K, typename V, auto P>
struct EntryLess
{
    static constexpr auto projection = P;

    template<typename Entry>
    constexpr bool operator()(const Entry& lhs, const Entry& rhs) const
    {
        using Projected = std::remove_cvref_t<std::invoke_result_t<decltype(P), const K&, const V&>>;
        // Projected twice rather than held, as in ProjectedLess.
        if (Less<Projected> {}(std::invoke(P, lhs.first, lhs.second), std::invoke(P, rhs.first, rhs.second)))
            return true;
        if (Less<Projected> {}(std::invoke(P, rhs.first, rhs.second), std::invoke(P, lhs.first, lhs.second)))
            return false;
        return Less<K> {}(lhs.first, rhs.first);
    }
};

namespace detail
{

template<typename C>
inline constexpr bool is_entry_less = false;
template<typename K, typename V, auto P>
inline constexpr bool is_entry_less<EntryLess<K, V, P>> = true;

template<typename C, typename T>
inline constexpr bool is_projected_less = false;
template<typename T, auto P>
inline constexpr bool is_projected_less<ProjectedLess<T, P>, T> = true;

/// What order Compare compares elements T by: its projection's result where it projects (order_by<P>), else T.
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

/// Comparator C orders T as sorted means: urd::Less, or std::less where T's operator< is what urd::Less uses; or the
/// order a request gave by order_by.
template<typename C, typename T>
concept natural_order =
    (orderable<T> && (std::same_as<C, Less<T>> || (std::same_as<C, std::less<T>> && std::totally_ordered<T>) )) || detail::is_projected_less<C, T>;

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
