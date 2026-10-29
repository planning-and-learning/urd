#pragma once

// Names of the containers a Store can resolve to: a short identifier, for test names, and a description, for
// the examples. Downstream code never needs them: it uses the Store and never names the representation.

#include "urd/store.hpp"

#include <array>
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
template<typename K, bool TrackWords>
struct name<urd::repr::DenseSet<K, TrackWords>>
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
struct name<urd::repr::IndexMap<K, V, false, false, true>>
{
    static constexpr std::string_view id = "stable_dense_map";
    static constexpr std::string_view value = "dense map over slots that never move (segments)";
};
template<typename K, typename V>
struct name<urd::repr::IndexMap<K, V, true>>
{
    static constexpr std::string_view id = "generation_map";
    static constexpr std::string_view value = "generation map (a slot per index, O(1) clear by generation)";
};
template<typename K, typename V>
struct name<urd::repr::IndexMap<K, V, true, false, true>>
{
    static constexpr std::string_view id = "stable_generation_map";
    static constexpr std::string_view value = "generation map over slots that never move (segments)";
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
struct name<urd::repr::LayeredPool<R>>
{
    static constexpr std::string_view id = "layered_pool";
    static constexpr std::string_view value = "layered pool (its own elements after a parent's, indices continuing)";
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
template<typename K, typename V, typename Order>
struct name<urd::repr::FlatEntryMap<K, V, Order>>
{
    static constexpr std::string_view id = "flat_entry_map";
    static constexpr std::string_view value = "flat entry map (entries in an order that reads their values, found by key)";
};
template<typename K, typename V>
struct name<urd::repr::RecyclingMap<K, V>>
{
    static constexpr std::string_view id = "recycling_map";
    static constexpr std::string_view value = "recycling map (std::map that keeps erased nodes and their values for reuse)";
};
template<typename K, typename V>
struct name<urd::repr::RecyclingHashMap<K, V>>
{
    static constexpr std::string_view id = "recycling_hash_map";
    static constexpr std::string_view value = "recycling hash map (std::unordered_map that keeps erased nodes and their values for reuse)";
};
#if defined(URD_HAS_GTL)
template<typename T>
struct name<urd::repr::ParallelHashSet<T>>
{
    static constexpr std::string_view id = "parallel_hash_set";
    static constexpr std::string_view value = "parallel hash set (gtl: shards, each behind its mutex)";
};
template<typename K, typename V>
struct name<urd::repr::ParallelHashMap<K, V>>
{
    static constexpr std::string_view id = "parallel_hash_map";
    static constexpr std::string_view value = "parallel hash map (gtl: shards, each behind its mutex; update and read under it)";
};
#endif
template<typename T, typename Index>
struct name<urd::repr::StableIndexedSet<T, Index>>
{
    static constexpr std::string_view id = "stable_indexed_set";
    static constexpr std::string_view value = "stable indexed set (segments that never move, a hash table of positions; no lock)";
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
template<typename K, typename V>
struct name<urd::repr::IndexMap<K, V, true, true>>
{
    static constexpr std::string_view id = "thread_safe_generation_map";
    static constexpr std::string_view value = "generation map, thread-safe updates (a slot and a spin lock per index)";
};
template<typename T, urd::repr::leaving Order, typename Compare>
struct name<urd::repr::Queue<T, Order, Compare>>
{
    static constexpr std::string_view id = Order == urd::repr::leaving::least ? "heap_by" : Order == urd::repr::leaving::first_in ? "fifo_by" : "lifo_by";
    static constexpr std::string_view value = "queue by a given order";
};
template<typename T, typename Compare>
struct name<urd::repr::AddressableHeap<T, Compare>>
{
    static constexpr std::string_view id = "addressable_heap";
    static constexpr std::string_view value = "addressable heap (push gives a handle to update or erase by)";
};
template<typename T, typename Compare>
struct name<urd::repr::BucketQueue<T, Compare>>
{
    static constexpr std::string_view id = "bucket_queue";
    static constexpr std::string_view value = "bucket queue (a bucket per integral priority)";
};
template<typename Inner>
struct name<urd::repr::Jagged<Inner, true>>
{
    static constexpr std::string_view id = "tracked_jagged";
    static constexpr std::string_view value = "jagged rows that track the rows handed out to change";
};
template<typename T>
struct name<urd::repr::RecordInterner<T, false>>
{
    static constexpr std::string_view id = "record_interner";
    static constexpr std::string_view value = "record interner (trivially copyable records in an indexed set)";
};
template<typename T>
struct name<urd::repr::RecordInterner<T, false, true>>
{
    static constexpr std::string_view id = "stable_record_interner";
    static constexpr std::string_view value = "record interner over segments that never move (no lock)";
};
template<typename T>
struct name<urd::repr::RecordInterner<T, true>>
{
    static constexpr std::string_view id = "thread_safe_record_interner";
    static constexpr std::string_view value = "record interner, thread-safe";
};
template<typename T>
struct name<urd::repr::ArenaInterner<T, false>>
{
    static constexpr std::string_view id = "arena_interner";
    static constexpr std::string_view value = "arena interner (records serialized by cista into a byte arena)";
};
template<typename T>
struct name<urd::repr::ArenaInterner<T, true>>
{
    static constexpr std::string_view id = "thread_safe_arena_interner";
    static constexpr std::string_view value = "arena interner, thread-safe";
};
template<typename Row>
struct name<urd::repr::RowInterner<Row, true>>
{
    static constexpr std::string_view id = "thread_safe_row_interner";
    static constexpr std::string_view value = "row interner, thread-safe";
};
template<typename T, typename C, typename A>
struct name<std::multiset<T, C, A>>
{
    static constexpr std::string_view id = "multiset";
    static constexpr std::string_view value = "std::multiset (balanced tree)";
};

/// The tags by name, in the order of urd::tag: for printing a request or a spec.
inline constexpr std::array<std::string_view, urd::detail::tag_count> tag_names {
    "any_order",
    "sorted",
    "insertion_ordered",
    "duplicates",
    "unique",
    "any_keys",
    "dense",
    "sparse",
    "moving_refs",
    "stable_refs",
    "any_layout",
    "contiguous",
    "any_representation",
    "canonical",
    "single_threaded",
    "concurrent",
    "any_memory",
    "retains",
    "reclaims",
    "any_clear",
    "proportional_clear",
    "constant_clear",
    "any_access",
    "fifo",
    "lifo",
    "by_priority",
    "any_scope",
    "scoped",
    "any_layering",
    "layered",
    "any_handle",
    "handles",
    "any_sharing",
    "structural_sharing",
    "any_direction",
    "bidirectional",
    "any_shape",
    "fixed_shape",
    "any_packing",
    "flat",
    "any_grouping",
    "partition",
    "any_tracking",
    "change_log",
    "immutable",
    "growable",
    "shrinkable",
    "may_change",
    "monotone",
    "frozen",
    "maybe_empty",
    "nonempty",
    "any_multiplicity",
    "functional",
    "multi",
    "by_key",
    "positional",
    "general",
    "small",
    "any_priority",
    "monotone_priority",
};
static_assert(tag_names[urd::detail::index(urd::tag::sorted)] == "sorted" && tag_names[urd::detail::index(urd::tag::concurrent)] == "concurrent"
              && tag_names[urd::detail::index(urd::tag::monotone_priority)] == "monotone_priority");
constexpr std::string_view tag_name(urd::tag of) { return tag_names[urd::detail::index(of)]; }

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
