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
template<typename K, bool KeepOrder>
struct name<urd::repr::SparseSet<K, KeepOrder>>
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
struct name<urd::repr::IndexedSet<T, std::uint32_t>>
{
    static constexpr std::string_view id = "indexed_set";
    static constexpr std::string_view value = "indexed set (a vector in insertion order, plus a hash table of positions)";
};
template<typename K, typename V>
struct name<urd::repr::CsrMultiMap<K, V>>
{
    static constexpr std::string_view id = "csr_multimap";
    static constexpr std::string_view value = "CSR multimap (each key's values together, in key order)";
};
template<typename K, typename V>
struct name<urd::repr::PostingMap<K, V>>
{
    static constexpr std::string_view id = "posting_map";
    static constexpr std::string_view value = "postings (a flat hash table of value vectors)";
};
template<typename T>
struct name<urd::repr::Fifo<T>>
{
    static constexpr std::string_view id = "fifo";
    static constexpr std::string_view value = "queue (a deque, first in first out)";
};
template<typename T>
struct name<urd::repr::Lifo<T>>
{
    static constexpr std::string_view id = "lifo";
    static constexpr std::string_view value = "stack (a vector, last in first out)";
};
template<typename T>
struct name<urd::repr::Heap<T>>
{
    static constexpr std::string_view id = "heap";
    static constexpr std::string_view value = "binary heap (a vector, least first)";
};
template<typename K, typename V>
struct name<urd::repr::ScopedMap<K, V>>
{
    static constexpr std::string_view id = "scoped_map";
    static constexpr std::string_view value = "scoped map (a flat hash table, scopes undone by a log)";
};
template<typename R>
struct name<urd::repr::Layered<R>>
{
    static constexpr std::string_view id = "layered";
    static constexpr std::string_view value = "layered (its own elements, then a parent's)";
};
template<typename R>
struct name<urd::repr::ChangeLogged<R>>
{
    static constexpr std::string_view id = "change_logged";
    static constexpr std::string_view value = "change-logged set (a flat hash set, plus what was added and removed)";
};
template<typename Row>
struct name<urd::repr::RowInterner<Row, false>>
{
    static constexpr std::string_view id = "row_interner";
    static constexpr std::string_view value = "row interner (each row once, in full)";
};
template<typename T, bool ThreadSafe>
struct name<urd::repr::SlotVector<T, ThreadSafe>>
{
    static constexpr std::string_view id = ThreadSafe ? "thread_safe_slot_vector" : "slot_vector";
    static constexpr std::string_view value = "slot vector (slots that never move, freed ones reused)";
};
template<typename K, typename V>
struct name<urd::repr::BidirectionalCsr<K, V>>
{
    static constexpr std::string_view id = "bidirectional_csr";
    static constexpr std::string_view value = "bidirectional CSR (each key's values and each value's keys together)";
};
template<typename Row, std::size_t Arity, bool ThreadSafe>
struct name<urd::repr::TreeBitsetSet<Row, Arity, ThreadSafe>>
{
    static constexpr std::string_view id = "tree_bitset_set";
    static constexpr std::string_view value = "tree bitset set (sets of indices as trees of blocks, equal subtrees once)";
};
template<typename T, typename Index>
struct name<urd::repr::ConcurrentIndexedSet<T, Index>>
{
    static constexpr std::string_view id = "concurrent_indexed_set";
    static constexpr std::string_view value = "concurrent indexed set (segments that never move, a concurrent table of positions)";
};
template<typename Row, std::size_t Arity, bool ThreadSafe>
struct name<urd::repr::TreeVectorSet<Row, Arity, ThreadSafe>>
{
    static constexpr std::string_view id = "tree_vector_set";
    static constexpr std::string_view value = "tree vector set (rows as trees of values, equal subtrees once)";
};
template<typename Keys, typename V>
struct name<urd::repr::OrderedMap<Keys, V>>
{
    static constexpr std::string_view id = "ordered_map";
    static constexpr std::string_view value = "ordered map (keys in an insertion-ordered set, values beside them)";
};
template<typename K>
struct name<urd::repr::DisjointSets<K>>
{
    static constexpr std::string_view id = "disjoint_sets";
    static constexpr std::string_view value = "disjoint sets (union-find by size, path halving)";
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
