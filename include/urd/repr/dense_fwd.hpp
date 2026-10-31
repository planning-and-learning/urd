#pragma once

// Containers for keys that are indices 0..n-1 (dense keys), declared with what the registry reads of them: a bitset,
// a sparse set, a slot-per-index map. Defined in urd/repr/dense_set.hpp, sparse_set.hpp and index_map.hpp. The registry
// names them for any element type, so they are unconstrained templates; their container_traits declare properties
// only for indexable keys.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

#include <type_traits>

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P dense_set(const T& t, bool track)
{
    if (!t.indexable)
        return {};
    return property_set::of({ tag::sorted, tag::unique, tag::dense, tag::canonical, tag::retains, track ? tag::proportional_clear : tag::any_clear });
}

constexpr P sparse_set(const T& t, bool keep_order)
{
    if (!t.indexable)
        return {};
    return property_set::of({ tag::unique,
                   tag::dense,
                   tag::positional,
                   tag::contiguous,
                   tag::retains,
                   t.trivially_destructible ? tag::constant_clear : tag::proportional_clear,
                   keep_order ? tag::insertion_ordered : tag::any_order });
}

constexpr P index_map(const T& key, bool stable)
{
    if (!key.indexable)
        return {};
    return property_set::of({ tag::sorted, tag::unique, tag::dense, tag::retains, references(stable) });
}

constexpr P generation_map(const T& key, const T& mapped, bool concurrent_, bool stable)
{
    if (!key.indexable || !mapped.default_initializable)
        return {};
    return property_set::of({ tag::sorted,
                   tag::unique,
                   tag::dense,
                   tag::retains,
                   tag::constant_clear,
                   references(stable),
                   threads(concurrent_) });
}

}  // namespace urd::detail::guarantees


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

namespace detail
{

/// dense set (a bitset over indices)
struct dense_set_decl
{
    static constexpr std::string_view id = "dense_set";
    static constexpr auto headers = header_list<1> { "urd/repr/dense_set.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (f.ordered)  // sorted by its own order only
            return {};
        const auto held = guarantees::dense_set(f.element, false);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::lin_keys;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 0.016;
    };
    static constexpr std::string_view description = "dense set (a bitset over indices)";
    static constexpr std::string_view holds = "U bits";
};
template<typename K>
struct declaration_of<repr::DenseSet<K, false>>
{
    using declaration = dense_set_decl;
    static constexpr request_features features = element_facts<K>();
};

}  // namespace detail

namespace detail
{

/// dense set tracking its words (a bitset over indices, cleared by the words it touched)
struct tracked_dense_set_decl
{
    static constexpr std::string_view id = "tracked_dense_set";
    static constexpr auto headers = header_list<1> { "urd/repr/dense_set.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (f.ordered)  // sorted by its own order only
            return {};
        const auto held = guarantees::dense_set(f.element, true);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 0.03;
    };
    static constexpr std::string_view description = "dense set tracking its words (a bitset over indices, cleared by the words it touched)";
    static constexpr std::string_view holds = "U bits + the indices of the words in use";
};
template<typename K>
struct declaration_of<repr::DenseSet<K, true>>
{
    using declaration = tracked_dense_set_decl;
    static constexpr request_features features = element_facts<K>();
};

}  // namespace detail

namespace detail
{

/// sparse set (members array plus index positions, O(1) clear)
struct sparse_set_decl
{
    static constexpr std::string_view id = "sparse_set";
    static constexpr auto headers = header_list<1> { "urd/repr/sparse_set.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        const auto held = guarantees::sparse_set(f.element, false);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::one;
        static constexpr cost iterate = costs::lin_n;
        static constexpr double words = 2;
    };
    static constexpr std::string_view description = "sparse set (members array plus index positions, O(1) clear)";
    static constexpr std::string_view holds = "n members + U positions";
};
template<typename K>
struct declaration_of<repr::SparseSet<K, false>>
{
    using declaration = sparse_set_decl;
    static constexpr request_features features = element_facts<K>();
};

}  // namespace detail

namespace detail
{

/// sparse set keeping insertion order (erase shifts the members after it)
struct ordered_sparse_set_decl
{
    static constexpr std::string_view id = "ordered_sparse_set";
    static constexpr auto headers = header_list<1> { "urd/repr/sparse_set.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        const auto held = guarantees::sparse_set(f.element, true);
        if (!held)
            return {};
        return offer_of(urd::kind::set, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::one;
        static constexpr cost iterate = costs::lin_n;
        static constexpr cost access = costs::one;
        static constexpr double words = 2;
    };
    static constexpr std::string_view description = "sparse set keeping insertion order (erase shifts the members after it)";
    static constexpr std::string_view holds = "n members in insertion order + U positions";
};
template<typename K>
struct declaration_of<repr::SparseSet<K, true>>
{
    using declaration = ordered_sparse_set_decl;
    static constexpr request_features features = element_facts<K>();
};

}  // namespace detail

namespace detail
{

/// dense map (a slot per index)
struct dense_map_decl
{
    static constexpr std::string_view id = "dense_map";
    static constexpr auto headers = header_list<1> { "urd/repr/index_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (f.ordered)  // sorted by its own order only
            return {};
        const auto held = guarantees::index_map(f.key, false);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::lin_keys;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 1.2;
    };
    static constexpr std::string_view description = "dense map (a slot per index)";
    static constexpr std::string_view holds = "a slot per index";
};
template<typename K, typename V>
struct declaration_of<repr::IndexMap<K, V, false>>
{
    using declaration = dense_map_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// dense map over slots that never move (segments)
struct stable_dense_map_decl
{
    static constexpr std::string_view id = "stable_dense_map";
    static constexpr auto headers = header_list<1> { "urd/repr/index_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (f.ordered)  // sorted by its own order only
            return {};
        const auto held = guarantees::index_map(f.key, true);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::lin_keys;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 1.3;
    };
    static constexpr std::string_view description = "dense map over slots that never move (segments)";
    static constexpr std::string_view holds = "a slot per index, in segments that never move";
};
template<typename K, typename V>
struct declaration_of<repr::IndexMap<K, V, false, false, true>>
{
    using declaration = stable_dense_map_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// generation map (a slot per index, O(1) clear by generation)
struct generation_map_decl
{
    static constexpr std::string_view id = "generation_map";
    static constexpr auto headers = header_list<1> { "urd/repr/index_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (f.ordered)  // sorted by its own order only
            return {};
        const auto held = guarantees::generation_map(f.key, f.mapped, false, false);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::one;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 1.6;
    };
    static constexpr std::string_view description = "generation map (a slot per index, O(1) clear by generation)";
    static constexpr std::string_view holds = "a slot and a generation per index";
};
template<typename K, typename V>
struct declaration_of<repr::IndexMap<K, V, true>>
{
    using declaration = generation_map_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// generation map over slots that never move (segments)
struct stable_generation_map_decl
{
    static constexpr std::string_view id = "stable_generation_map";
    static constexpr auto headers = header_list<1> { "urd/repr/index_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (f.ordered)  // sorted by its own order only
            return {};
        const auto held = guarantees::generation_map(f.key, f.mapped, false, true);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::one;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 1.7;
    };
    static constexpr std::string_view description = "generation map over slots that never move (segments)";
    static constexpr std::string_view holds = "a slot and a generation per index, in segments that never move";
};
template<typename K, typename V>
struct declaration_of<repr::IndexMap<K, V, true, false, true>>
{
    using declaration = stable_generation_map_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// generation map, thread-safe updates (a slot and a spin lock per index)
struct thread_safe_generation_map_decl
{
    static constexpr std::string_view id = "thread_safe_generation_map";
    static constexpr auto headers = header_list<1> { "urd/repr/index_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (f.ordered)  // sorted by its own order only
            return {};
        const auto held = guarantees::generation_map(f.key, f.mapped, true, true);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::one;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 1.8;
    };
    static constexpr std::string_view description = "generation map, thread-safe updates (a slot and a spin lock per index)";
    static constexpr std::string_view holds = "a slot, a generation and a lock byte per index, in segments";
};
template<typename K, typename V>
struct declaration_of<repr::IndexMap<K, V, true, true>>
{
    using declaration = thread_safe_generation_map_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

}  // namespace urd
