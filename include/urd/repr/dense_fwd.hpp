#pragma once

// Containers for keys that are indices 0..n-1 (dense keys), declared with what the registry reads of them: a bitset,
// a sparse set, a slot-per-index map. Defined in urd/repr/dense.hpp, which urd/use/dense.hpp includes. The registry
// names them for any element type, so they are unconstrained templates; their container_traits declare properties
// only for indexable keys.

#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/synopsis.hpp"
#include "urd/traits.hpp"

#include <type_traits>

namespace urd::repr
{

/// A set of indices as a bitset: a bit per key below the largest, iterated in key order; with TrackWords, clear
/// zeroes only the words that are not zero.
template<typename K, bool TrackWords = false>
class DenseSet;

/// A set of indices as a sparse set (Briggs and Torczon): the members in one array, their places by index; with
/// KeepOrder, the members stay in insertion order.
template<typename K, bool KeepOrder = false>
class SparseSet;

/// A map from indices to values, a slot per index below the largest key. Generations: clear starts a new one, O(1).
/// Stable: the slots never move. ThreadSafe (with Stable and Generations): update and read from threads at once.
template<typename K, typename V, bool Generations, bool ThreadSafe = false, bool Stable = ThreadSafe>
class IndexMap;

}  // namespace urd::repr

namespace urd
{

/// A bit per key: sorted by index, which urd::Less orders as it does indices. Clear zeroes every word, or with
/// TrackWords only those not zero, at most one per element: time in proportion to the elements.
template<indexable K, bool TrackWords>
struct container_traits<repr::DenseSet<K, TrackWords>> :
    properties_of<sorted, unique, dense, canonical, shrinkable, retains, std::conditional_t<TrackWords, proportional_clear, any_clear>>
{
};

/// Members in one array; clear only forgets them, which is O(1) where they need no destructor.
/// With KeepOrder, in insertion order.
template<indexable K, bool KeepOrder>
struct container_traits<repr::SparseSet<K, KeepOrder>> :
    properties_of<unique,
                  dense,
                  positional,
                  contiguous,
                  shrinkable,
                  retains,
                  std::conditional_t<std::is_trivially_destructible_v<K>, constant_clear, proportional_clear>,
                  std::conditional_t<KeepOrder, insertion_ordered, any_order>>
{
};

/// A slot per key, in key order: sorted; erase frees what the value owns (std::optional::reset), clear scans every
/// slot. Stable slots never move: the values keep their addresses.
template<indexable K, typename V, bool Stable>
struct container_traits<repr::IndexMap<K, V, false, false, Stable>> :
    properties_of<sorted, unique, functional, dense, shrinkable, retains, std::conditional_t<Stable, stable_refs, moving_refs>>
{
};

/// Clear starts a new generation; stale slots keep their values' storage until written again.
template<indexable K, typename V, bool ThreadSafe, bool Stable>
    requires detail::slot_value<V>
struct container_traits<repr::IndexMap<K, V, true, ThreadSafe, Stable>> :
    properties_of<sorted,
                  unique,
                  functional,
                  dense,
                  shrinkable,
                  retains,
                  constant_clear,
                  std::conditional_t<Stable, stable_refs, moving_refs>,
                  std::conditional_t<ThreadSafe, concurrent, single_threaded>>
{
};

template<typename K, bool TrackWords>
inline constexpr auto defined_in<repr::DenseSet<K, TrackWords>> = detail::fixed_string("urd/use/dense.hpp");
template<typename K, bool KeepOrder>
inline constexpr auto defined_in<repr::SparseSet<K, KeepOrder>> = detail::fixed_string("urd/use/dense.hpp");
template<typename K, typename V, bool Generations, bool ThreadSafe, bool Stable>
inline constexpr auto defined_in<repr::IndexMap<K, V, Generations, ThreadSafe, Stable>> = detail::fixed_string("urd/use/dense.hpp");

}  // namespace urd

namespace urd
{

/// dense_set.
template<typename K>
struct container_costs<repr::DenseSet<K, false>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost lookup = costs::one_t;
    static constexpr cost ordered_lookup = costs::lin_keys;
    static constexpr cost erase = costs::one_t;
    static constexpr cost clear = costs::lin_keys;
    static constexpr cost iterate = costs::lin_keys;
    static constexpr double words = 0.016;
};

/// tracked_dense_set.
template<typename K>
struct container_costs<repr::DenseSet<K, true>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost lookup = costs::one_t;
    static constexpr cost ordered_lookup = costs::lin_keys;
    static constexpr cost erase = costs::one_t;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_keys;
    static constexpr double words = 0.03;
};

/// sparse_set.
template<typename K>
struct container_costs<repr::SparseSet<K, false>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost lookup = costs::one_t;
    static constexpr cost erase = costs::one_t;
    static constexpr cost clear = costs::one_t;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr double words = 2;
};

/// ordered_sparse_set.
template<typename K>
struct container_costs<repr::SparseSet<K, true>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost lookup = costs::one_t;
    static constexpr cost erase = costs::lin_nt;
    static constexpr cost clear = costs::one_t;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost access = costs::one_t;
    static constexpr double words = 2;
};

/// dense_map.
template<typename K, typename V>
struct container_costs<repr::IndexMap<K, V, false>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost lookup = costs::one_t;
    static constexpr cost ordered_lookup = costs::lin_keys;
    static constexpr cost erase = costs::one_t;
    static constexpr cost clear = costs::lin_keys;
    static constexpr cost iterate = costs::lin_keys;
    static constexpr double words = 1.2;
};

/// stable_dense_map.
template<typename K, typename V>
struct container_costs<repr::IndexMap<K, V, false, false, true>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost lookup = costs::one_t;
    static constexpr cost ordered_lookup = costs::lin_keys;
    static constexpr cost erase = costs::one_t;
    static constexpr cost clear = costs::lin_keys;
    static constexpr cost iterate = costs::lin_keys;
    static constexpr double words = 1.3;
};

/// generation_map.
template<typename K, typename V>
struct container_costs<repr::IndexMap<K, V, true>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost lookup = costs::one_t;
    static constexpr cost ordered_lookup = costs::lin_keys;
    static constexpr cost erase = costs::one_t;
    static constexpr cost clear = costs::one_t;
    static constexpr cost iterate = costs::lin_keys;
    static constexpr double words = 1.6;
};

/// stable_generation_map.
template<typename K, typename V>
struct container_costs<repr::IndexMap<K, V, true, false, true>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost lookup = costs::one_t;
    static constexpr cost ordered_lookup = costs::lin_keys;
    static constexpr cost erase = costs::one_t;
    static constexpr cost clear = costs::one_t;
    static constexpr cost iterate = costs::lin_keys;
    static constexpr double words = 1.7;
};

/// thread_safe_generation_map.
template<typename K, typename V>
struct container_costs<repr::IndexMap<K, V, true, true>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost lookup = costs::one_t;
    static constexpr cost ordered_lookup = costs::lin_keys;
    static constexpr cost erase = costs::one_t;
    static constexpr cost clear = costs::one_t;
    static constexpr cost iterate = costs::lin_keys;
    static constexpr double words = 1.8;
};

}  // namespace urd

namespace urd
{

template<typename K>
struct container_synopsis<repr::DenseSet<K, false>>
{
    static constexpr std::string_view id = "dense_set";
    static constexpr std::string_view description = "dense set (a bitset over indices)";
    static constexpr std::string_view holds = "U bits";
};

template<typename K>
struct container_synopsis<repr::DenseSet<K, true>>
{
    static constexpr std::string_view id = "tracked_dense_set";
    static constexpr std::string_view description = "dense set tracking its words (a bitset over indices, cleared by the words it touched)";
    static constexpr std::string_view holds = "U bits + the indices of the words in use";
};

template<typename K>
struct container_synopsis<repr::SparseSet<K, false>>
{
    static constexpr std::string_view id = "sparse_set";
    static constexpr std::string_view description = "sparse set (members array plus index positions, O(1) clear)";
    static constexpr std::string_view holds = "n members + U positions";
};

template<typename K>
struct container_synopsis<repr::SparseSet<K, true>>
{
    static constexpr std::string_view id = "ordered_sparse_set";
    static constexpr std::string_view description = "sparse set keeping insertion order (erase shifts the members after it)";
    static constexpr std::string_view holds = "n members in insertion order + U positions";
};

template<typename K, typename V>
struct container_synopsis<repr::IndexMap<K, V, false>>
{
    static constexpr std::string_view id = "dense_map";
    static constexpr std::string_view description = "dense map (a slot per index)";
    static constexpr std::string_view holds = "a slot per index";
};

template<typename K, typename V>
struct container_synopsis<repr::IndexMap<K, V, false, false, true>>
{
    static constexpr std::string_view id = "stable_dense_map";
    static constexpr std::string_view description = "dense map over slots that never move (segments)";
    static constexpr std::string_view holds = "a slot per index, in segments that never move";
};

template<typename K, typename V>
struct container_synopsis<repr::IndexMap<K, V, true>>
{
    static constexpr std::string_view id = "generation_map";
    static constexpr std::string_view description = "generation map (a slot per index, O(1) clear by generation)";
    static constexpr std::string_view holds = "a slot and a generation per index";
};

template<typename K, typename V>
struct container_synopsis<repr::IndexMap<K, V, true, false, true>>
{
    static constexpr std::string_view id = "stable_generation_map";
    static constexpr std::string_view description = "generation map over slots that never move (segments)";
    static constexpr std::string_view holds = "a slot and a generation per index, in segments that never move";
};

template<typename K, typename V>
struct container_synopsis<repr::IndexMap<K, V, true, true>>
{
    static constexpr std::string_view id = "thread_safe_generation_map";
    static constexpr std::string_view description = "generation map, thread-safe updates (a slot and a spin lock per index)";
    static constexpr std::string_view holds = "a slot, a generation and a lock byte per index, in segments";
};

}  // namespace urd
