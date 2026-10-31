#pragma once

// A map with scopes (pop_scope undoes every change since the matching push_scope), declared with what the registry
// reads of it, over a flat hash table, a tree or a dense map. Defined in urd/repr/scoped.hpp, which
// urd/use/scopes.hpp includes; the map it scopes comes with the map's family.

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/elements.hpp"
#include "urd/repr/dense_fwd.hpp"
#include "urd/traits.hpp"

#include <boost/unordered/unordered_flat_map_fwd.hpp>
#include <map>

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

/// As Map, with scopes undone by a log.
template<typename Map>
    requires container_traits<Map>::declared && has<container_traits<Map>, unique, functional>
struct container_traits<repr::Scoped<Map>> : with_t<container_traits<Map>, scoped>
{
};
template<typename Map>
struct parts<repr::Scoped<Map>>
{
    using type = detail::type_list<Map>;
};

template<typename Map>
inline constexpr auto defined_in<repr::Scoped<Map>> = detail::fixed_string("urd/use/scopes.hpp");

}  // namespace urd

namespace urd
{

/// scoped_map.
template<typename K, typename V>
struct container_costs<repr::ScopedMap<K, V>>
{
    static constexpr cost insert = costs::one_ae;
    static constexpr cost lookup = costs::one_e;
    static constexpr cost erase = costs::one_e;
    static constexpr cost clear = costs::lin_capacity;
    static constexpr cost iterate = costs::lin_capacity;
    static constexpr cost scope = costs::lin_changes;
    static constexpr double words = 2.3;
};

/// scoped_tree_map.
template<typename K, typename V>
struct container_costs<repr::ScopedTreeMap<K, V>>
{
    static constexpr cost insert = costs::log_n;
    static constexpr cost lookup = costs::log_n;
    static constexpr cost ordered_lookup = costs::log_n;
    static constexpr cost erase = costs::log_n;
    static constexpr cost clear = costs::lin_nt;
    static constexpr cost iterate = costs::lin_nt;
    static constexpr cost scope = costs::nlog_changes;
    static constexpr double words = 6;
};

/// scoped_dense_map.
template<typename K, typename V>
struct container_costs<repr::ScopedDenseMap<K, V>>
{
    static constexpr cost insert = costs::one_at;
    static constexpr cost lookup = costs::one_t;
    static constexpr cost ordered_lookup = costs::lin_keys;
    static constexpr cost erase = costs::one_t;
    static constexpr cost clear = costs::lin_keys;
    static constexpr cost iterate = costs::lin_keys;
    static constexpr cost scope = costs::lin_changes_t;
    static constexpr double words = 2.2;
};

}  // namespace urd

namespace urd
{

template<typename K, typename V>
struct container_synopsis<repr::ScopedMap<K, V>>
{
    static constexpr std::string_view id = "scoped_map";
    static constexpr std::string_view description = "scoped map (a flat hash table, scopes undone by a log)";
    static constexpr std::string_view holds = "+ a log entry per change; pop_scope undoes them";
};

template<typename K, typename V>
struct container_synopsis<repr::ScopedTreeMap<K, V>>
{
    static constexpr std::string_view id = "scoped_tree_map";
    static constexpr std::string_view description = "scoped tree map (std::map, scopes undone by a log)";
    static constexpr std::string_view holds = "+ a log entry per change; pop_scope undoes them";
};

template<typename K, typename V>
struct container_synopsis<repr::ScopedDenseMap<K, V>>
{
    static constexpr std::string_view id = "scoped_dense_map";
    static constexpr std::string_view description = "scoped dense map (a slot per index, scopes undone by a log)";
    static constexpr std::string_view holds = "+ a log entry per change; pop_scope undoes them";
};

}  // namespace urd
