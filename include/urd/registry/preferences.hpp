#pragma once

// The registry's order: every container it may choose, in order of preference, the first that serves a request
// being chosen (urd/registry/select.hpp). Adding a container is adding it here, after declaring it (its traits, costs
// and synopsis) and aliasing it with urd's hash, equality and order (urd/registry/aliases.hpp).

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
/// tables, and only a request that promises dense keys reaches them at all (demands_met: a container that stores by
/// index demands keys that are indices, whatever its place in the list); the indexed set, after them too, only a
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
#if defined(URD_HAS_GTL)
                                              ParallelHashSetOf,
                                              ParallelNodeHashSetOf,
#endif
                                              ConcurrentIndexedSet,
                                              SlotVector,
                                              ThreadSafeSlotVector,
                                              repr::DisjointSets>;
using map_preference = map_candidates<FlatHashMapOf,
                                      HashMapOf,
                                      PostingMapOf,
                                      HashMultiMapOf,
#if defined(URD_HAS_GTL)
                                      ParallelHashMapOf,
                                      ParallelNodeHashMapOf,
#endif
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

/// Whether R is a copy of a registered container made for tests/scaling: such a copy serves nothing (serves, last).
template<typename R>
inline constexpr bool registry_copy = false;

#if defined(URD_REGISTRY_COPIES)
/// For tests/scaling only: copy I of container R, a type of its own with R's traits, which selection checks as far as
/// R and then turns down (registry_copy). URD_REGISTRY_COPIES copies of every candidate come before the candidates
/// themselves: the registry grown that many times over, with every request resolving as before.
template<std::size_t I, typename R>
struct RegistryCopy
{
};
template<std::size_t I, typename R>
inline constexpr bool registry_copy<RegistryCopy<I, R>> = true;
template<std::size_t I, typename... Rs>
using copies_of = type_list<RegistryCopy<I, Rs>...>;
template<typename... Rs, std::size_t... I>
auto with_copies(type_list<Rs...> /*candidates*/, std::index_sequence<I...> /*copies*/) -> typename join<copies_of<I, Rs...>..., type_list<Rs...>>::type;
template<typename List>
using registry_grown =
    decltype(detail::with_copies(List {}, std::make_index_sequence<URD_REGISTRY_COPIES>()));  // qualified: lookup by argument would complete every candidate
#else
template<typename List>
using registry_grown = List;
#endif

/// The registered containers for spec S, instantiated with its element type, or a map's key and value types.
template<typename S, typename Candidates>
struct instantiated;
template<typename S, template<typename...> class... Cs>
struct instantiated<S, element_candidates<Cs...>>
{
    using type = registry_grown<type_list<Cs<typename S::element_type>...>>;
};
template<typename S, template<typename, typename> class... Cs>
struct instantiated<S, map_candidates<Cs...>>
{
    using type = registry_grown<type_list<Cs<typename S::key_type, typename S::mapped_type>...>>;
};
template<typename S, template<typename, typename> class... Cs>
struct instantiated<S, ordered_element_candidates<Cs...>>
{
    using type = registry_grown<type_list<Cs<typename S::element_type, typename S::order_type>...>>;
};
template<typename S, template<typename, typename, typename> class... Cs>
struct instantiated<S, ordered_map_candidates<Cs...>>
{
    using type = registry_grown<type_list<Cs<typename S::key_type, typename S::mapped_type, typename S::order_type>...>>;
};

}  // namespace urd::detail

#if defined(URD_REGISTRY_COPIES)
namespace urd
{
template<std::size_t I, typename R>
struct container_traits<detail::RegistryCopy<I, R>> : container_traits<R>
{
};
}  // namespace urd
#endif
