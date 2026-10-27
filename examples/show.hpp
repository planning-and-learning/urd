#pragma once

// Prints what a Store request resolved to, for the examples. Downstream code never needs this: it uses the
// Store and never names the representation.

#include "urd/store.hpp"

#include <iostream>
#include <map>
#include <set>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace example
{

template<typename R>
struct name
{
    static constexpr std::string_view value = "?";
};
template<typename T, typename A>
struct name<std::vector<T, A>>
{
    static constexpr std::string_view value = "std::vector";
};
template<typename T, typename C, typename A>
struct name<std::set<T, C, A>>
{
    static constexpr std::string_view value = "std::set (balanced tree)";
};
template<typename T, typename H, typename E, typename A>
struct name<std::unordered_set<T, H, E, A>>
{
    static constexpr std::string_view value = "std::unordered_set (hash table)";
};
template<typename T, typename C, typename A>
struct name<boost::container::flat_set<T, C, A>>
{
    static constexpr std::string_view value = "boost::container::flat_set (sorted vector)";
};
template<typename T, typename C, typename A>
struct name<boost::container::flat_multiset<T, C, A>>
{
    static constexpr std::string_view value = "boost::container::flat_multiset (sorted vector)";
};
template<typename T, std::size_t N, typename I>
struct name<urd::repr::SegmentedVector<T, N, false, I>>
{
    static constexpr std::string_view value = "segmented vector (elements never move)";
};
template<typename T, std::size_t N, typename I>
struct name<urd::repr::SegmentedVector<T, N, true, I>>
{
    static constexpr std::string_view value = "segmented vector, thread-safe appends";
};
template<typename K, typename V, typename C, typename A>
struct name<std::map<K, V, C, A>>
{
    static constexpr std::string_view value = "std::map (balanced tree)";
};
template<typename K, typename V, typename C, typename A>
struct name<std::multimap<K, V, C, A>>
{
    static constexpr std::string_view value = "std::multimap (balanced tree)";
};
template<typename K, typename V, typename H, typename E, typename A>
struct name<std::unordered_map<K, V, H, E, A>>
{
    static constexpr std::string_view value = "std::unordered_map (hash table)";
};
template<typename K, typename V, typename H, typename E, typename A>
struct name<std::unordered_multimap<K, V, H, E, A>>
{
    static constexpr std::string_view value = "std::unordered_multimap (hash table)";
};
template<typename K, typename V, typename C, typename A>
struct name<boost::container::flat_map<K, V, C, A>>
{
    static constexpr std::string_view value = "boost::container::flat_map (sorted vector)";
};
template<typename K, typename V, typename C, typename A>
struct name<boost::container::flat_multimap<K, V, C, A>>
{
    static constexpr std::string_view value = "boost::container::flat_multimap (sorted vector)";
};
template<typename T, typename H, typename E, typename A>
struct name<boost::unordered_flat_set<T, H, E, A>>
{
    static constexpr std::string_view value = "boost::unordered_flat_set (open-addressing hash table)";
};
template<typename K, typename V, typename H, typename E, typename A>
struct name<boost::unordered_flat_map<K, V, H, E, A>>
{
    static constexpr std::string_view value = "boost::unordered_flat_map (open-addressing hash table)";
};
template<typename Inner>
struct name<urd::repr::Jagged<Inner>>
{
    static constexpr std::string_view value = "jagged array (CSR: offsets + one data array)";
};
template<typename Inner>
struct name<urd::repr::RowPool<Inner>>
{
    static constexpr std::string_view value = "row pool (cleared rows are kept for reuse)";
};

/// Prints `request` and the container urd chose for Store S.
template<typename S>
void show(std::string_view request)
{
    std::cout << "  " << request << "\n      -> " << name<typename S::representation>::value << '\n';
}

}  // namespace example
