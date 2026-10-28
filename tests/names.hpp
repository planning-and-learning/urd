#pragma once

// Names of the containers a Store can resolve to: a short identifier, for test names, and a description, for
// the examples. Downstream code never needs them: it uses the Store and never names the representation.

#include "urd/store.hpp"

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace urd::test
{

template<typename R>
struct name
{
    static constexpr std::string_view id = "other";
    static constexpr std::string_view value = "a container of its own";
};
template<typename T, typename A>
struct name<std::vector<T, A>>
{
    static constexpr std::string_view id = "vector";
    static constexpr std::string_view value = "std::vector";
};
template<typename T, typename C, typename A>
struct name<std::set<T, C, A>>
{
    static constexpr std::string_view id = "set";
    static constexpr std::string_view value = "std::set (balanced tree)";
};
template<typename T, typename H, typename E, typename A>
struct name<std::unordered_set<T, H, E, A>>
{
    static constexpr std::string_view id = "unordered_set";
    static constexpr std::string_view value = "std::unordered_set (hash table)";
};
template<typename T, typename C, typename A>
struct name<boost::container::flat_set<T, C, A>>
{
    static constexpr std::string_view id = "flat_set";
    static constexpr std::string_view value = "boost::container::flat_set (sorted vector)";
};
template<typename T, typename C, typename A>
struct name<boost::container::flat_multiset<T, C, A>>
{
    static constexpr std::string_view id = "flat_multiset";
    static constexpr std::string_view value = "boost::container::flat_multiset (sorted vector)";
};
template<typename T, std::size_t N, typename I>
struct name<urd::repr::SegmentedVector<T, N, false, I>>
{
    static constexpr std::string_view id = "segmented_vector";
    static constexpr std::string_view value = "segmented vector (elements never move)";
};
template<typename T, std::size_t N, typename I>
struct name<urd::repr::SegmentedVector<T, N, true, I>>
{
    static constexpr std::string_view id = "concurrent_segmented_vector";
    static constexpr std::string_view value = "segmented vector, thread-safe appends";
};
template<typename K, typename V, typename C, typename A>
struct name<std::map<K, V, C, A>>
{
    static constexpr std::string_view id = "map";
    static constexpr std::string_view value = "std::map (balanced tree)";
};
template<typename K, typename V, typename C, typename A>
struct name<std::multimap<K, V, C, A>>
{
    static constexpr std::string_view id = "multimap";
    static constexpr std::string_view value = "std::multimap (balanced tree)";
};
template<typename K, typename V, typename H, typename E, typename A>
struct name<std::unordered_map<K, V, H, E, A>>
{
    static constexpr std::string_view id = "unordered_map";
    static constexpr std::string_view value = "std::unordered_map (hash table)";
};
template<typename K, typename V, typename H, typename E, typename A>
struct name<std::unordered_multimap<K, V, H, E, A>>
{
    static constexpr std::string_view id = "unordered_multimap";
    static constexpr std::string_view value = "std::unordered_multimap (hash table)";
};
template<typename K, typename V, typename C, typename A>
struct name<boost::container::flat_map<K, V, C, A>>
{
    static constexpr std::string_view id = "flat_map";
    static constexpr std::string_view value = "boost::container::flat_map (sorted vector)";
};
template<typename K, typename V, typename C, typename A>
struct name<boost::container::flat_multimap<K, V, C, A>>
{
    static constexpr std::string_view id = "flat_multimap";
    static constexpr std::string_view value = "boost::container::flat_multimap (sorted vector)";
};
template<typename T, typename H, typename E, typename A>
struct name<boost::unordered_flat_set<T, H, E, A>>
{
    static constexpr std::string_view id = "unordered_flat_set";
    static constexpr std::string_view value = "boost::unordered_flat_set (open-addressing hash table)";
};
template<typename K, typename V, typename H, typename E, typename A>
struct name<boost::unordered_flat_map<K, V, H, E, A>>
{
    static constexpr std::string_view id = "unordered_flat_map";
    static constexpr std::string_view value = "boost::unordered_flat_map (open-addressing hash table)";
};
template<typename Inner>
struct name<urd::repr::Jagged<Inner>>
{
    static constexpr std::string_view id = "jagged";
    static constexpr std::string_view value = "jagged array (CSR: offsets + one data array)";
};
template<typename Inner>
struct name<urd::repr::RowPool<Inner>>
{
    static constexpr std::string_view id = "row_pool";
    static constexpr std::string_view value = "row pool (cleared rows are kept for reuse)";
};
template<typename K>
struct name<urd::repr::DenseSet<K>>
{
    static constexpr std::string_view id = "dense_set";
    static constexpr std::string_view value = "dense set (a bitset over indices)";
};
template<typename K>
struct name<urd::repr::SparseSet<K>>
{
    static constexpr std::string_view id = "sparse_set";
    static constexpr std::string_view value = "sparse set (members array plus index positions, O(1) clear)";
};
template<typename K, typename V>
struct name<urd::repr::IndexMap<K, V, false>>
{
    static constexpr std::string_view id = "dense_map";
    static constexpr std::string_view value = "dense map (a slot per index)";
};
template<typename K, typename V>
struct name<urd::repr::IndexMap<K, V, true>>
{
    static constexpr std::string_view id = "generation_map";
    static constexpr std::string_view value = "generation map (a slot per index, O(1) clear by generation)";
};
template<typename T>
struct name<urd::repr::IndexedSet<T>>
{
    static constexpr std::string_view id = "indexed_set";
    static constexpr std::string_view value = "indexed set (a vector in insertion order, plus a hash table of positions)";
};
/// Names typed tests by container: a Store as "Store_" and its representation, with the index for uniqueness.
struct Names
{
    template<typename T>
    static std::string GetName(int index)
    {
        auto result = std::string();
        if constexpr (urd::is_store<T>)
            result = "Store_" + std::string(name<typename T::representation>::id);
        else
            result = std::string(name<T>::id);
        return result + "_" + std::to_string(index);
    }
};

}  // namespace urd::test
