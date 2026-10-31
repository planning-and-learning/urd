#pragma once

// A map in insertion order over an insertion-ordered set of keys, declared with what the registry reads of it.
// Defined in urd/repr/ordered_map.hpp; its keys (an indexed set or a sparse set) in theirs.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/repr/indexed_fwd.hpp"
#include "urd/traits.hpp"

#include <type_traits>

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P ordered_map(const P& keys, const T& mapped)
{
    if (!keys || !keys->has(tag::insertion_ordered) || !keys->has(tag::unique))
        return {};
    const auto clear = (mapped.trivially_destructible && keys->has(tag::constant_clear)) ? tag::constant_clear :
                       keys->has(tag::proportional_clear)                                ? tag::proportional_clear :
                                                                                           tag::any_clear;
    return with(*keys, { tag::functional, tag::by_key, tag::any_layout, clear });
}

}  // namespace urd::detail::guarantees


namespace urd::repr
{

/// A map in insertion order: its keys in Keys, an insertion-ordered set whose positions index the values, and the
/// values in a vector beside them.
template<typename Keys, typename V>
class OrderedMap;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// ordered map (keys in an insertion-ordered set, values beside them)
struct ordered_map_decl
{
    static constexpr std::string_view id = "ordered_map";
    static constexpr auto headers = header_list<2> { "urd/repr/indexed.hpp", "urd/repr/ordered_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::ordered_map(guarantees::indexed_set(f.key), f.mapped);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs : indexed_set_decl::operation_costs
    {
        using part = indexed_set_decl;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr double words = indexed_set_decl::operation_costs::words + 1;
    };
    static constexpr std::string_view description = "ordered map (keys in an insertion-ordered set, values beside them)";
    static constexpr std::string_view holds = "its key set + n values in order";
};
template<typename K, typename V>
struct declaration_of<repr::OrderedMap<repr::IndexedSet<K>, V>>
{
    using declaration = ordered_map_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// dense ordered map (indices in an insertion-ordered sparse set, values beside them)
struct dense_ordered_map_decl
{
    static constexpr std::string_view id = "dense_ordered_map";
    static constexpr auto headers = header_list<2> { "urd/repr/sparse_set.hpp", "urd/repr/ordered_map.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::ordered_map(guarantees::sparse_set(f.key, true), f.mapped);
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs : ordered_sparse_set_decl::operation_costs
    {
        using part = ordered_sparse_set_decl;
        static constexpr cost erase = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr double words = ordered_sparse_set_decl::operation_costs::words + 1;
    };
    static constexpr std::string_view description = "dense ordered map (indices in an insertion-ordered sparse set, values beside them)";
    static constexpr std::string_view holds = "its sparse set + n values in order";
};
template<typename K, typename V>
struct declaration_of<repr::OrderedMap<repr::SparseSet<K, true>, V>>
{
    using declaration = dense_ordered_map_decl;
    static constexpr request_features features = map_facts<repr::SparseSet<K, true>, V>();
};

}  // namespace detail

}  // namespace urd
