#pragma once

#include "urd/properties.hpp"
#include "urd/repr/ops.hpp"

#include <concepts>
#include <cstddef>
#include <functional>
#include <set>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace urd::repr
{

/// The standard library's sets: built from any elements, so assume() has nothing to trust.
template<typename R, typename E>
struct std_set_ops
{
    [[nodiscard]] static R make(std::vector<E> elements) { return R(elements.begin(), elements.end()); }
    [[nodiscard]] static R assume(std::vector<E> elements) { return make(std::move(elements)); }
    static bool insert(R& set, const E& element) { return set.insert(element).second; }
    static std::size_t erase(R& set, const E& element) { return set.erase(element); }
    [[nodiscard]] static bool contains(const R& set, const E& element) { return set.contains(element); }
};

template<typename K>
struct ops<std::set<K>> : std_set_ops<std::set<K>, K>
{
};

template<typename K>
struct ops<std::unordered_set<K>> : std_set_ops<std::unordered_set<K>, K>
{
};

/// A balanced search tree: sorted, O(log n) to insert and find, stable references.
template<typename S>
struct tree_set
{
    using type = std::set<typename S::key_type>;
    static constexpr std::string_view name = "std::set";
    static constexpr bool erases = true;
    static constexpr bool accepts = S::is_set && std::totally_ordered<typename S::key_type>;
    static constexpr props guarantees = props::sorted | props::unique | props::stable_refs | props::sparse;
    static constexpr int rank = includes(S::flags, props::frozen) || S::work == workload::small ? 2 : 1;
};

template<typename K>
concept hashable = requires(const K& key) {
    { std::hash<K> {}(key) } -> std::convertible_to<std::size_t>;
};

/// A hash table: O(1) expected to insert and find, in no particular order.
template<typename S>
struct hash_set
{
    using type = std::unordered_set<typename S::key_type>;
    static constexpr std::string_view name = "std::unordered_set";
    static constexpr bool erases = true;
    static constexpr bool accepts = S::is_set && hashable<typename S::key_type>;
    static constexpr props guarantees = props::unique | props::stable_refs | props::sparse;
    static constexpr int rank = S::work == workload::small ? 3 : (includes(S::flags, props::frozen) ? 2 : 0);
};

}  // namespace urd::repr
