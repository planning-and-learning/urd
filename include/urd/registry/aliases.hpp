#pragma once

// The containers the registry chooses among, as it instantiates them: each with urd's hash, equality and order, so that
// any hashable or orderable element works, and with its other template parameters fixed. Declarations only: each
// container's header is its declaration (urd/repr/*_fwd.hpp and the like), so naming them completes nothing.

#include "urd/elements.hpp"
#include "urd/repr/change_logged_fwd.hpp"
#include "urd/repr/core/dependent_false.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/repr/disjoint_sets_fwd.hpp"
#include "urd/repr/entry_ordered_fwd.hpp"
#include "urd/repr/flat.hpp"
#include "urd/repr/flat_hash.hpp"
#include "urd/repr/grouped_fwd.hpp"
#include "urd/repr/indexed_fwd.hpp"
#include "urd/repr/layered_fwd.hpp"
#include "urd/repr/ordered_map_fwd.hpp"
#include "urd/repr/parallel.hpp"
#include "urd/repr/pools_fwd.hpp"
#include "urd/repr/queues_fwd.hpp"
#include "urd/repr/records_fwd.hpp"
#include "urd/repr/recycling_fwd.hpp"
#include "urd/repr/rows_fwd.hpp"
#include "urd/repr/scoped_fwd.hpp"
#include "urd/repr/segmented_fwd.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/traits.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <ranges>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace urd::detail
{

// The containers, with urd's hash, equality and order, so that any hashable or orderable element works and nothing
// has to be specialized in namespace std.
template<typename T>
using HashSet = std::unordered_set<T, Hash<T>, EqualTo<T>>;
template<typename T>
using FlatHashSet = boost::unordered_flat_set<T, Hash<T>, EqualTo<T>>;
template<typename T>
using TreeSet = std::set<T, Less<T>>;
template<typename T>
using FlatSet = boost::container::flat_set<T, Less<T>>;
template<typename T>
using FlatMultiSet = boost::container::flat_multiset<T, Less<T>>;

template<typename K, typename V>
using HashMapOf = std::unordered_map<K, V, Hash<K>, EqualTo<K>>;
template<typename K, typename V>
using HashMultiMapOf = std::unordered_multimap<K, V, Hash<K>, EqualTo<K>>;
template<typename K, typename V>
using FlatHashMapOf = boost::unordered_flat_map<K, V, Hash<K>, EqualTo<K>>;
template<typename K, typename V>
using TreeMapOf = std::map<K, V, Less<K>>;
template<typename K, typename V>
using TreeMultiMapOf = std::multimap<K, V, Less<K>>;
template<typename K, typename V>
using FlatMapOf = boost::container::flat_map<K, V, Less<K>>;
template<typename K, typename V>
using FlatMultiMapOf = boost::container::flat_multimap<K, V, Less<K>>;

template<typename K, typename V>
using DenseMapOf = repr::IndexMap<K, V, false>;
template<typename K, typename V>
using StableDenseMapOf = repr::IndexMap<K, V, false, false, true>;
template<typename K, typename V>
using GenerationMapOf = repr::IndexMap<K, V, true>;
template<typename K, typename V>
using StableGenerationMapOf = repr::IndexMap<K, V, true, false, true>;
template<typename K, typename V>
using ThreadSafeGenerationMapOf = repr::IndexMap<K, V, true, true>;

template<typename T>
using DenseSetOf = repr::DenseSet<T>;
template<typename T>
using TrackedDenseSet = repr::DenseSet<T, true>;
template<typename T>
using SparseSetOf = repr::SparseSet<T>;
template<typename T>
using OrderedSparseSet = repr::SparseSet<T, true>;
template<typename K, typename V>
using OrderedMapOf = repr::OrderedMap<repr::IndexedSet<K>, V>;
template<typename K, typename V>
using DenseOrderedMapOf = repr::OrderedMap<repr::SparseSet<K, true>, V>;
template<typename K, typename V>
using PostingMapOf = repr::PostingMap<K, V>;
template<typename K, typename V>
using CsrMultiMapOf = repr::CsrMultiMap<K, V>;
template<typename K, typename V>
using BidirectionalCsrOf = repr::BidirectionalCsr<K, V>;
template<typename K, typename V>
using DenseMultiMapOf = repr::DenseMultiMap<K, V>;

template<typename K, typename V>
using ScopedMapOf = repr::ScopedMap<K, V>;
template<typename K, typename V>
using ScopedTreeMapOf = repr::ScopedTreeMap<K, V>;
template<typename K, typename V>
using ScopedDenseMapOf = repr::ScopedDenseMap<K, V>;
template<typename T>
using SlotVector = repr::SlotVector<T>;
template<typename T>
using ThreadSafeSlotVector = repr::SlotVector<T, true>;
template<typename Row>
using TreeVectorOf = repr::TreeVectorSet<Row>;
template<typename Row>
using TreeBitsetOf = repr::TreeBitsetSet<Row>;
template<typename Row>
using RowInternerOf = repr::RowInterner<Row>;
template<typename Row>
using ThreadSafeRowInterner = repr::RowInterner<Row, true>;
template<typename Row>
using ThreadSafeTreeVectorOf = repr::TreeVectorSet<Row, 4, true>;
template<typename Row>
using ThreadSafeTreeBitsetOf = repr::TreeBitsetSet<Row, 4, true>;
template<typename T>
using ConcurrentIndexedSet = repr::ConcurrentIndexedSet<T>;
template<typename T>
using IndexedSetOf = repr::IndexedSet<T>;
template<typename T>
using StableIndexedSet = repr::StableIndexedSet<T>;
template<typename Inner>
using JaggedOf = repr::Jagged<Inner>;
template<typename Inner>
using TrackedJaggedOf = repr::Jagged<Inner, true>;
template<typename T>
using BucketQueueOf = repr::BucketQueue<T>;
template<typename T>
using AddressableHeapOf = repr::AddressableHeap<T>;
// gtl's sharded tables: the concurrent sets and maps by hashed keys.
template<typename T>
using ParallelHashSetOf = repr::ParallelHashSet<T>;
template<typename K, typename V>
using ParallelHashMapOf = repr::ParallelHashMap<K, V>;
template<typename T>
using ParallelNodeHashSetOf = repr::ParallelNodeHashSet<T>;
template<typename K, typename V>
using ParallelNodeHashMapOf = repr::ParallelNodeHashMap<K, V>;
template<typename T>
using RecyclingHashSetOf = repr::RecyclingHashSet<T>;
template<typename T>
using RecyclingTreeSetOf = repr::RecyclingTreeSet<T>;
template<typename K, typename V>
using RecyclingHashMapOf = repr::RecyclingHashMap<K, V>;
template<typename K, typename V>
using RecyclingMapOf = repr::RecyclingMap<K, V>;
template<typename T>
using ChangeLoggedHashSet = repr::ChangeLogged<FlatHashSet<T>>;
template<typename T>
using ChangeLoggedDenseSet = repr::ChangeLogged<TrackedDenseSet<T>>;
template<typename T>
using ChangeLoggedTreeSet = repr::ChangeLogged<TreeSet<T>>;
template<typename T>
using ChangeLoggedNodeSet = repr::ChangeLogged<HashSet<T>>;
template<typename T>
using LayeredHashSet = repr::Layered<FlatHashSet<T>>;
template<typename K, typename V>
using LayeredHashMapOf = repr::Layered<FlatHashMapOf<K, V>>;
template<typename K, typename V>
using LayeredDenseMapOf = repr::Layered<DenseMapOf<K, V>>;
template<typename T>
using LayeredNodeSet = repr::Layered<HashSet<T>>;
template<typename K, typename V>
using LayeredNodeMapOf = repr::Layered<HashMapOf<K, V>>;
template<typename K, typename V>
using LayeredGenerationMapOf = repr::Layered<GenerationMapOf<K, V>>;

template<typename T>
using SegmentedVector = repr::SegmentedVector<T>;

template<typename T>
using RecordInternerOf = repr::RecordInterner<T>;
template<typename T>
using StableRecordInterner = repr::RecordInterner<T, false, true>;
template<typename T>
using ThreadSafeRecordInterner = repr::RecordInterner<T, true>;
template<typename T>
using ArenaInternerOf = repr::ArenaInterner<T>;
template<typename T>
using ThreadSafeArenaInterner = repr::ArenaInterner<T, true>;
template<typename Row>
using LayeredRowInternerOf = repr::LayeredPool<repr::RowInterner<Row>>;
template<typename Row>
using ThreadSafeLayeredRowInterner = repr::LayeredPool<repr::RowInterner<Row, true>>;
template<typename T>
using LayeredRecordInternerOf = repr::LayeredPool<repr::RecordInterner<T>>;
template<typename T>
using StableLayeredRecordInterner = repr::LayeredPool<repr::RecordInterner<T, false, true>>;
template<typename T>
using ThreadSafeLayeredRecordInterner = repr::LayeredPool<repr::RecordInterner<T, true>>;
template<typename T>
using LayeredArenaInternerOf = repr::LayeredPool<repr::ArenaInterner<T>>;
template<typename T>
using ThreadSafeLayeredArenaInterner = repr::LayeredPool<repr::ArenaInterner<T, true>>;
template<typename T>
using ConcurrentSegmentedVector = repr::SegmentedVector<T, repr::default_first_segment_size, true>;

/// The containers that take a comparator, with it as their last parameter: what a request's order_by instantiates.
template<typename T, typename C>
using TreeSetBy = std::set<T, C>;
template<typename T, typename C>
using FlatSetBy = boost::container::flat_set<T, C>;
template<typename T, typename C>
using FlatMultiSetBy = boost::container::flat_multiset<T, C>;
template<typename T, typename C>
using HeapBy = repr::Queue<T, repr::leaving::least, C>;
template<typename T, typename C>
using BucketQueueBy = repr::BucketQueue<T, C>;
template<typename T, typename C>
using AddressableHeapBy = repr::AddressableHeap<T, C>;
template<typename K, typename V, typename C>
using TreeMapBy = std::map<K, V, C>;
template<typename K, typename V, typename C>
using TreeMultiMapBy = std::multimap<K, V, C>;
template<typename K, typename V, typename C>
using FlatMapBy = boost::container::flat_map<K, V, C>;
template<typename K, typename V, typename C>
using FlatMultiMapBy = boost::container::flat_multimap<K, V, C>;
template<typename K, typename V, typename C>
using RecyclingMapBy = repr::Recycling<std::map<K, V, C>>;
/// Maps ordered by their values too (order_by<P> with P(key, value)).
template<typename K, typename V, typename C>
using FlatEntryMapBy = repr::FlatEntryMap<K, V, C>;

}  // namespace urd::detail
