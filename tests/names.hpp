#pragma once

// Names of the containers a Store can resolve to: a short identifier, for test names, and a description, for
// the examples. Downstream code never needs them: it uses the Store and never names the representation.

#include "urd/store.hpp"

#include <algorithm>
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
struct name<urd::repr::DenseMultiMap<K, V>>
{
    static constexpr std::string_view id = "dense_multimap";
    static constexpr std::string_view value = "dense multimap (a vector of values per index, growing)";
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
struct name<urd::repr::RingFifo<T>>
{
    static constexpr std::string_view id = "ring_fifo";
    static constexpr std::string_view value = "ring fifo (one vector used as a ring, kept on pop and clear)";
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
template<typename K, typename V>
struct name<urd::repr::ScopedTreeMap<K, V>>
{
    static constexpr std::string_view id = "scoped_tree_map";
    static constexpr std::string_view value = "scoped tree map (std::map, scopes undone by a log)";
};
template<typename K, typename V>
struct name<urd::repr::ScopedDenseMap<K, V>>
{
    static constexpr std::string_view id = "scoped_dense_map";
    static constexpr std::string_view value = "scoped dense map (a slot per index, scopes undone by a log)";
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
template<typename T>
struct name<urd::repr::Layered<std::unordered_set<T, urd::Hash<T>, urd::EqualTo<T>>>>
{
    static constexpr std::string_view id = "layered_node_set";
    static constexpr std::string_view value = "layered node set (std::unordered_set over a parent's)";
};
template<typename K, typename V>
struct name<urd::repr::Layered<std::unordered_map<K, V, urd::Hash<K>, urd::EqualTo<K>>>>
{
    static constexpr std::string_view id = "layered_node_map";
    static constexpr std::string_view value = "layered node map (std::unordered_map over a parent's)";
};
template<typename K, typename V>
struct name<urd::repr::Layered<urd::repr::IndexMap<K, V, true>>>
{
    static constexpr std::string_view id = "layered_generation_map";
    static constexpr std::string_view value = "layered generation map (a slot per index, O(1) clear, over a parent's)";
};
template<typename R>
struct name<urd::repr::ChangeLogged<R>>
{
    static constexpr std::string_view id = "change_logged";
    static constexpr std::string_view value = "change-logged set (a flat hash set, plus what was added and removed)";
};
template<typename T>
struct name<urd::repr::ChangeLogged<urd::repr::DenseSet<T, true>>>
{
    static constexpr std::string_view id = "change_logged_dense_set";
    static constexpr std::string_view value = "change-logged dense set (a bitset, plus what was added and removed)";
};
template<typename T>
struct name<urd::repr::ChangeLogged<std::set<T, urd::Less<T>>>>
{
    static constexpr std::string_view id = "change_logged_tree_set";
    static constexpr std::string_view value = "change-logged tree set (std::set, plus what was added and removed)";
};
template<typename T>
struct name<urd::repr::ChangeLogged<std::unordered_set<T, urd::Hash<T>, urd::EqualTo<T>>>>
{
    static constexpr std::string_view id = "change_logged_node_set";
    static constexpr std::string_view value = "change-logged node set (std::unordered_set, plus what was added and removed)";
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
template<typename T>
struct name<urd::repr::RecyclingHashSet<T>>
{
    static constexpr std::string_view id = "recycling_hash_set";
    static constexpr std::string_view value = "recycling hash set (std::unordered_set that keeps erased nodes for reuse)";
};
template<typename T>
struct name<urd::repr::RecyclingTreeSet<T>>
{
    static constexpr std::string_view id = "recycling_tree_set";
    static constexpr std::string_view value = "recycling tree set (std::set that keeps erased nodes for reuse)";
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
template<typename T>
struct name<urd::repr::ParallelNodeHashSet<T>>
{
    static constexpr std::string_view id = "parallel_node_hash_set";
    static constexpr std::string_view value = "parallel node hash set (gtl: shards behind mutexes, a node per element that never moves)";
};
template<typename K, typename V>
struct name<urd::repr::ParallelNodeHashMap<K, V>>
{
    static constexpr std::string_view id = "parallel_node_hash_map";
    static constexpr std::string_view value = "parallel node hash map (gtl: shards behind mutexes, a node per entry that never moves)";
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
    static constexpr std::string_view id = Order == urd::repr::leaving::least    ? std::string_view("heap_by") :
                                           Order == urd::repr::leaving::first_in ? std::string_view("fifo_by") :
                                                                                   std::string_view("lifo_by");
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

/// What a tag is to the one who writes a request, for share/urd/vocabulary.json (tests/vocabulary_table.cpp):
/// - guarantee: a container declares it, a request may ask for it;
/// - demand: the caller states it about its data, and a container may rely on it (dense: demands_met);
/// - kind: it makes the Store a queue, a pool or a partition;
/// - request: only a request states it, the Store's interface enforces it;
/// - hint: it reorders the candidates, never changes a guarantee.
/// The description follows the tag's comment in urd/traits.hpp; the phrases are ways a need is put in plain words.
/// A category's root has an empty entry: it promises nothing and is never written.
struct tag_entry
{
    std::string_view role;
    std::string_view description;
    std::array<std::string_view, 4> phrases;
};

/// In the order of urd::tag, as tag_names.
inline constexpr std::array<tag_entry, urd::detail::tag_count> tag_vocabulary { {
    {},  // any_order
    { "guarantee", "Iterated in ascending order by urd::Less.", { "sorted", "in order", "ordered by key", "range queries" } },
    { "guarantee", "Iterated in the order elements were first inserted.", { "insertion order", "first come first listed", "keeps the order added" } },
    {},  // duplicates
    { "guarantee", "No two elements are equal.", { "no duplicates", "distinct", "each at most once" } },
    {},  // any_keys
    { "demand", "The keys are 0..n-1, or nearly so: indices filling their range.", { "ids", "indices", "vertices numbered 0..n-1", "small key range" } },
    { "demand", "The keys are few compared to their range.", { "few keys in a large range", "scattered keys", "arbitrary keys" } },
    {},  // moving_refs
    { "guarantee",
      "A reference to an element stays valid until that element is removed.",
      { "pointers stay valid", "stable addresses", "references survive inserts" } },
    {},  // any_layout
    { "guarantee", "The elements lie next to each other in memory.", { "contiguous", "cache friendly", "as a span", "one array" } },
    {},  // any_representation
    { "guarantee", "Equal contents have equal representations.", { "canonical", "same contents same bytes", "hashable by contents" } },
    {},  // single_threaded
    { "guarantee",
      "Threads share it: inserts from several threads at once, and the reads each kind allows beside them.",
      { "thread safe", "shared between threads", "parallel inserts" } },
    {},  // any_memory
    { "guarantee", "Storage freed by erase and clear is kept for reuse.", { "no reallocation when refilled", "reuse memory", "keeps capacity" } },
    { "guarantee", "Erase and clear return the storage of what they remove.", { "frees memory", "shrinks", "gives memory back" } },
    {},  // any_clear
    { "guarantee", "Clear takes time in proportion to the elements present.", { "cheap clear", "cleared often" } },
    { "guarantee", "Clear takes constant time, as with a generation counter.", { "reset every round", "cleared per search", "O(1) clear" } },
    {},  // any_access
    { "kind", "A queue: elements leave in the order they came.", { "queue", "fifo", "breadth first", "worklist" } },
    { "kind", "A stack: the last element pushed leaves first.", { "stack", "lifo", "depth first" } },
    { "kind", "A priority queue: the least element leaves first.", { "priority queue", "heap", "best first", "open list" } },
    {},  // any_scope
    { "guarantee", "push_scope and pop_scope: popping undoes every change since the matching push.", { "nested scopes", "undo on backtrack", "symbol table" } },
    {},  // any_layering
    { "guarantee",
      "layer_over(parent): lookups find the parent's elements too, which it neither owns nor changes.",
      { "falls through to a parent", "overlay", "phase over phase" } },
    {},  // any_handle
    { "kind",
      "A pool: insert gives a handle; with unique, equal elements get equal handles.",
      { "interning", "handles", "deduplicated rows", "hash consing" } },
    {},  // any_sharing
    { "guarantee", "Parts that are equal across rows are stored once.", { "rows differing in a few places", "persistent", "shared structure" } },
    {},  // any_direction
    { "guarantee", "A map that answers by value too: keys(v) gives the keys that map to v.", { "predecessors", "reverse edges", "inverse map" } },
    {},  // any_shape
    { "guarantee", "Rows whose number and lengths are set at construction; only their contents change.", { "fixed rows", "matrix", "table of known shape" } },
    {},  // any_packing
    { "guarantee",
      "Each record is one self-contained block of bytes, frozen once stored and read by index.",
      { "flat records", "serialized", "one block per record" } },
    {},  // any_grouping
    { "kind", "A partition: elements fall into disjoint classes that merge joins.", { "union find", "disjoint sets", "equivalence classes" } },
    {},  // any_tracking
    { "guarantee", "added() and removed(): the elements changed since clear_changes().", { "delta", "what changed", "semi-naive evaluation" } },
    {},  // immutable
    { "guarantee", "Offers insert.", { "grows" } },
    { "guarantee", "Offers insert, erase and clear.", { "add and remove" } },
    {},  // may_change
    { "request", "Only grows: nothing is ever removed.", { "append only", "only grows", "never erased" } },
    { "request", "No change after construction; assign rebuilds it whole.", { "built once", "read only", "immutable after building" } },
    {},  // maybe_empty
    { "request", "At least one element.", { "never empty", "nonempty" } },
    {},  // any_multiplicity
    { "request", "At most one value per key: a map.", { "map", "one value per key", "lookup table" } },
    { "request", "Any number of values per key: a multimap.", { "multimap", "many values per key", "adjacency lists" } },
    {},  // by_key
    { "request", "Identified by position 0..n-1, in order.", { "by position", "numbered as inserted", "interner" } },
    {},  // general
    { "hint", "A few elements, or a few at a time: compact containers first.", { "few elements", "small", "tiny" } },
    {},  // any_priority
    { "demand", "A priority queue never pushed below the last popped.", { "dijkstra", "non-negative lengths", "event simulation" } },
} };
// An entry for every tag that is no root, and none for a root: a tag added without its entry, or an entry out of
// place, fails here.
static_assert(
    []
    {
        for (std::size_t each = 0; each < urd::detail::tag_count; ++each)
        {
            const auto of = static_cast<urd::tag>(each);
            if (tag_vocabulary[each].role.empty() != (urd::detail::root_of(of) == of))
                return false;
        }
        return true;
    }(),
    "tag_vocabulary: an entry per tag that is no root, in the order of urd::tag");
static_assert(tag_vocabulary[urd::detail::index(urd::tag::sorted)].description.starts_with("Iterated in ascending")
              && tag_vocabulary[urd::detail::index(urd::tag::monotone_priority)].role == "demand");

/// The themes, the areas a reader may care about: they group the categories and have no rules of their own.
struct theme_entry
{
    std::string_view id;
    std::string_view title;
    std::string_view summary;
};

/// In the order a request is best thought through: what the Store is and what the data is like decide the most.
inline constexpr std::array<theme_entry, 6> theme_vocabulary { {
    { "kind", "What it is", "Whether the Store is a set, map or sequence, a queue, a pool or a partition, and how it is reached." },
    { "data", "What the data is like", "Promises about the elements: which keys, duplicates, emptiness, priorities, how many." },
    { "order", "Order", "In which order the elements are iterated, and whether they are reached by position." },
    { "change", "Change over time", "Whether the Store changes after building, how it is cleared, and what it remembers of changes." },
    { "memory", "Memory and layout", "Where elements live: stable references, one array, packed records, shared rows, freed memory." },
    { "threads", "Threads", "Whether several threads change the Store at once." },
} };

/// A category to the one who writes a request: its theme and the question its tags answer. Whether a request may
/// state its tags comes from the code (urd::detail::statable).
struct category_entry
{
    std::string_view theme;
    std::string_view question;
};

/// In the order of urd::tag: an entry per root, none for a tag that is no root.
inline constexpr std::array<category_entry, urd::detail::tag_count> category_vocabulary { {
    { "order", "In which order is it iterated?" },  // any_order
    {},
    {},
    { "data", "Duplicates allowed?" },  // duplicates
    {},
    { "data", "Which keys?" },  // any_keys
    {},
    {},
    { "memory", "Do references stay valid?" },  // moving_refs
    {},
    { "memory", "One array?" },  // any_layout
    {},
    { "memory", "Same contents, same bytes?" },  // any_representation
    {},
    { "threads", "Shared between threads?" },  // single_threaded
    {},
    { "memory", "What happens to freed memory?" },  // any_memory
    {},
    {},
    { "change", "How is it cleared?" },  // any_clear
    {},
    {},
    { "kind", "How are elements taken out?" },  // any_access
    {},
    {},
    {},
    { "change", "Nested scopes?" },  // any_scope
    {},
    { "change", "Layered over a parent?" },  // any_layering
    {},
    { "kind", "Reached by handle?" },  // any_handle
    {},
    { "memory", "Rows share structure?" },  // any_sharing
    {},
    { "kind", "Looked up both ways?" },  // any_direction
    {},
    { "kind", "Rows of a fixed shape?" },  // any_shape
    {},
    { "memory", "Records packed flat?" },  // any_packing
    {},
    { "kind", "Groups of keys?" },  // any_grouping
    {},
    { "change", "Tracks what changed?" },  // any_tracking
    {},
    { "change", "Grows or shrinks? (follows from monotone and frozen)" },  // immutable
    {},
    {},
    { "change", "Does it change after building?" },  // may_change
    {},
    {},
    { "data", "Can it be empty?" },  // maybe_empty
    {},
    { "kind", "Values per key?" },  // any_multiplicity
    {},
    {},
    { "order", "Reached by position?" },  // by_key
    {},
    { "data", "How many elements?" },  // general
    {},
    { "data", "How do priorities move?" },  // any_priority
    {},
} };
// An entry for every root and none for another tag, each naming a theme: a category added without its entry, or an
// entry out of place, fails here.
static_assert(
    []
    {
        for (std::size_t each = 0; each < urd::detail::tag_count; ++each)
        {
            const auto of = static_cast<urd::tag>(each);
            const auto& entry = category_vocabulary[each];
            if (entry.question.empty() != (urd::detail::root_of(of) != of))
                return false;
            if (!entry.question.empty() && std::ranges::none_of(theme_vocabulary, [&](const theme_entry& theme) { return theme.id == entry.theme; }))
                return false;
        }
        return true;
    }(),
    "category_vocabulary: an entry per root, naming a theme of theme_vocabulary, in the order of urd::tag");
static_assert(category_vocabulary[urd::detail::index(urd::tag::any_keys)].question == "Which keys?"
              && category_vocabulary[urd::detail::index(urd::tag::single_threaded)].theme == "threads");

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
