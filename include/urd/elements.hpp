#pragma once

#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/semantics/less.hpp"

#include <concepts>
#include <functional>

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

}  // namespace urd
