#pragma once

// The registry's order as types: every container it may choose, in order of preference. Selection scans the rows,
// which follow these lists entry for entry (urd/registry/rows.hpp); these are walked by the tests and generators that
// read every container (urd/registry/candidates.hpp). Adding a container is adding it here and its row there, after
// declaring it (its traits, costs and synopsis) and aliasing it with urd's hash, equality and order
// (urd/registry/aliases.hpp).

#include "urd/registry/aliases.hpp"

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

/// Container templates over the element type (sets, sequences, rows), in order of preference.
template<template<typename...> class... Cs>
struct element_candidates
{
};
/// Container templates over the key and the value type (maps), in order of preference.
template<template<typename, typename> class... Cs>
struct map_candidates
{
};

/// Every representation, in order of preference; the first that serves a request is chosen. A container is
/// registered by adding it to one of these lists; what it can serve, its container_traits say. The order
/// follows docs/BENCHMARKS.md: an open-addressing hash table is fastest at insert, lookup and clear-and-refill
/// at every size, so it comes first wherever order is not asked for; node tables and segments remain for stable_refs;
/// where order is asked for, a tree serves elements inserted over time. The dense containers come after the hash
/// tables, and only a request that promises dense keys reaches them at all (dense declares itself demanded: a container
/// that stores by index needs keys that are indices, whatever its place in the list); the indexed set, after them too, only a
/// request for insertion order or positions. Frozen rows are flattened first; rows
/// are kept for reuse only where their memory must be.
using element_preference = element_candidates<JaggedOf,
                                              TrackedJaggedOf,
                                              std::vector,
                                              FlatHashSet,
                                              HashSet,
                                              TreeSet,
                                              FlatMultiSet,
                                              IndexedSetOf,
                                              FlatSet,
                                              RecyclingHashSetOf,
                                              RecyclingTreeSetOf,
                                              DenseSetOf,
                                              TrackedDenseSet,
                                              SparseSetOf,
                                              StableIndexedSet,
                                              OrderedSparseSet,
                                              SegmentedVector,
                                              ConcurrentSegmentedVector,
                                              repr::RowPool,
                                              repr::Fifo,
                                              repr::RingFifo,
                                              repr::Lifo,
                                              BucketQueueOf,
                                              repr::Heap,
                                              AddressableHeapOf,
                                              LayeredHashSet,
                                              LayeredNodeSet,
                                              ChangeLoggedHashSet,
                                              ChangeLoggedDenseSet,
                                              ChangeLoggedNodeSet,
                                              ChangeLoggedTreeSet,
                                              RowInternerOf,
                                              TreeVectorOf,
                                              TreeBitsetOf,
                                              ThreadSafeRowInterner,
                                              ThreadSafeTreeVectorOf,
                                              ThreadSafeTreeBitsetOf,
                                              RecordInternerOf,
                                              ArenaInternerOf,
                                              StableRecordInterner,
                                              ThreadSafeRecordInterner,
                                              ThreadSafeArenaInterner,
                                              LayeredRowInternerOf,
                                              LayeredRecordInternerOf,
                                              LayeredArenaInternerOf,
                                              StableLayeredRecordInterner,
                                              ThreadSafeLayeredRowInterner,
                                              ThreadSafeLayeredRecordInterner,
                                              ThreadSafeLayeredArenaInterner,
                                              ParallelHashSetOf,
                                              ParallelNodeHashSetOf,
                                              ConcurrentIndexedSet,
                                              SlotVector,
                                              ThreadSafeSlotVector,
                                              repr::DisjointSets>;
using map_preference = map_candidates<FlatHashMapOf,
                                      HashMapOf,
                                      PostingMapOf,
                                      HashMultiMapOf,
                                      ParallelHashMapOf,
                                      ParallelNodeHashMapOf,
                                      TreeMapOf,
                                      TreeMultiMapOf,
                                      FlatMultiMapOf,
                                      FlatMapOf,
                                      RecyclingHashMapOf,
                                      RecyclingMapOf,
                                      DenseMapOf,
                                      StableDenseMapOf,
                                      GenerationMapOf,
                                      StableGenerationMapOf,
                                      ThreadSafeGenerationMapOf,
                                      OrderedMapOf,
                                      DenseOrderedMapOf,
                                      CsrMultiMapOf,
                                      BidirectionalCsrOf,
                                      DenseMultiMapOf,
                                      ScopedMapOf,
                                      ScopedTreeMapOf,
                                      ScopedDenseMapOf,
                                      LayeredHashMapOf,
                                      LayeredNodeMapOf,
                                      LayeredDenseMapOf,
                                      LayeredGenerationMapOf>;

/// The containers that take a comparator, in order of preference (as in the lists above): where a request gives
/// order_by, only they can serve it, instantiated with its comparator.
template<template<typename, typename> class... Cs>
struct ordered_element_candidates
{
};
template<template<typename, typename, typename> class... Cs>
struct ordered_map_candidates
{
};
using ordered_element_preference = ordered_element_candidates<TreeSetBy, FlatMultiSetBy, FlatSetBy, BucketQueueBy, HeapBy, AddressableHeapBy>;
using ordered_map_preference = ordered_map_candidates<TreeMapBy, TreeMultiMapBy, FlatMultiMapBy, FlatMapBy, RecyclingMapBy>;
/// Maps ordered by their values too (order_by<P> with P(key, value)).
using value_ordered_map_preference = ordered_map_candidates<FlatEntryMapBy>;

template<typename... Lists>
struct join
{
    using type = type_list<>;
};
template<typename... Ts>
struct join<type_list<Ts...>>
{
    using type = type_list<Ts...>;
};
template<typename... As, typename... Bs, typename... Lists>
struct join<type_list<As...>, type_list<Bs...>, Lists...> : join<type_list<As..., Bs...>, Lists...>
{
};

/// The registered containers for spec S, instantiated with its element type, or a map's key and value types.
template<typename S, typename Candidates>
struct instantiated;
template<typename S, template<typename...> class... Cs>
struct instantiated<S, element_candidates<Cs...>>
{
    using type = type_list<Cs<typename S::element_type>...>;
};
template<typename S, template<typename, typename> class... Cs>
struct instantiated<S, map_candidates<Cs...>>
{
    using type = type_list<Cs<typename S::key_type, typename S::mapped_type>...>;
};
template<typename S, template<typename, typename> class... Cs>
struct instantiated<S, ordered_element_candidates<Cs...>>
{
    using type = type_list<Cs<typename S::element_type, typename S::order_type>...>;
};
template<typename S, template<typename, typename, typename> class... Cs>
struct instantiated<S, ordered_map_candidates<Cs...>>
{
    using type = type_list<Cs<typename S::key_type, typename S::mapped_type, typename S::order_type>...>;
};

}  // namespace urd::detail
