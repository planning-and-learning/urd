#pragma once

// A map with scopes (pop_scope undoes every change since the matching push_scope), declared with what the registry
// reads of it, over a flat hash table, a tree or a dense map. Defined in urd/repr/scoped.hpp; the map it
// scopes in its own header.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/repr/flat_hash.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/traits.hpp"

#include <boost/unordered/unordered_flat_map_fwd.hpp>
#include <map>

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P scoped(const P& part)
{
    if (!part || !part->has(tag::unique) || !part->has(tag::functional))
        return {};
    return with(*part, { tag::scoped });
}

}  // namespace urd::detail::guarantees


namespace urd::repr
{

/// Map, with a log of each change made inside a scope, undone when the scope ends. The registry names it for any key
/// type, so it is an unconstrained template; its container_traits declare properties where Map's do.
template<typename Map>
class Scoped;

/// Scopes over a flat hash table.
template<typename K, typename V>
using ScopedMap = Scoped<boost::unordered_flat_map<K, V, Hash<K>, EqualTo<K>>>;
/// Scopes over a tree: sorted.
template<typename K, typename V>
using ScopedTreeMap = Scoped<std::map<K, V, Less<K>>>;
/// Scopes over a slot per index: dense keys.
template<typename K, typename V>
using ScopedDenseMap = Scoped<IndexMap<K, V, false>>;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// scoped map (a flat hash table, scopes undone by a log)
struct scoped_map_decl
{
    static constexpr std::string_view id = "scoped_map";
    static constexpr auto headers = header_list<2> { "urd/repr/unordered_flat_map.hpp", "urd/repr/scoped.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::scoped(guarantees::flat_hash_map(f.key));
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        using part = unordered_flat_map_decl;
        static constexpr cost insert = costs::one_ae;
        static constexpr cost lookup = costs::one_e;
        static constexpr cost erase = costs::one_e;
        static constexpr cost clear = costs::lin_capacity;
        static constexpr cost iterate = costs::lin_capacity;
        static constexpr cost scope = costs::lin_changes;
        static constexpr double words = 2.3;
    };
    static constexpr std::string_view description = "scoped map (a flat hash table, scopes undone by a log)";
    static constexpr std::string_view holds = "+ a log entry per change; pop_scope undoes them";
};
template<typename K, typename V>
struct declaration_of<repr::ScopedMap<K, V>>
{
    using declaration = scoped_map_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// scoped tree map (std::map, scopes undone by a log)
struct scoped_tree_map_decl
{
    static constexpr std::string_view id = "scoped_tree_map";
    static constexpr auto headers = header_list<2> { "urd/repr/std_map.hpp", "urd/repr/scoped.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::scoped(guarantees::tree_map(f.key, f));
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        using part = map_decl;
        static constexpr cost insert = costs::log_n;
        static constexpr cost lookup = costs::log_n;
        static constexpr cost ordered_lookup = costs::log_n;
        static constexpr cost erase = costs::log_n;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr cost scope = costs::nlog_changes;
        static constexpr double words = 6;
    };
    static constexpr std::string_view description = "scoped tree map (std::map, scopes undone by a log)";
    static constexpr std::string_view holds = "+ a log entry per change; pop_scope undoes them";
};
template<typename K, typename V>
struct declaration_of<repr::ScopedTreeMap<K, V>>
{
    using declaration = scoped_tree_map_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

namespace detail
{

/// scoped dense map (a slot per index, scopes undone by a log)
struct scoped_dense_map_decl
{
    static constexpr std::string_view id = "scoped_dense_map";
    static constexpr auto headers = header_list<2> { "urd/repr/index_map.hpp", "urd/repr/scoped.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::scoped(guarantees::index_map(f.key, false));
        if (!held)
            return {};
        return offer_of(urd::kind::map, *held, f);
    }
    struct operation_costs
    {
        using part = dense_map_decl;
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::lin_keys;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr cost scope = costs::lin_changes;
        static constexpr double words = 2.2;
    };
    static constexpr std::string_view description = "scoped dense map (a slot per index, scopes undone by a log)";
    static constexpr std::string_view holds = "+ a log entry per change; pop_scope undoes them";
};
template<typename K, typename V>
struct declaration_of<repr::ScopedDenseMap<K, V>>
{
    using declaration = scoped_dense_map_decl;
    static constexpr request_features features = map_facts<K, V>();
};

}  // namespace detail

}  // namespace urd
