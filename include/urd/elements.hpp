#pragma once

#include <concepts>
#include <cstddef>
#include <functional>

namespace urd
{

// What a container needs of its elements to keep its properties. A container declares a property only for
// elements that allow it: std::set<K> is sorted only if K is orderable.

/// Ordered by operator<, consistently with ==: what sorted means.
template<typename T>
concept orderable = std::totally_ordered<T>;

/// Hashed by std::hash and compared by ==: what a hash table needs to find an element.
template<typename T>
concept hashable = std::equality_comparable<T> && requires(const T& element) {
    { std::hash<T> {}(element) } -> std::convertible_to<std::size_t>;
};

}  // namespace urd
