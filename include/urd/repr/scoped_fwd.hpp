#pragma once

// A map with scopes (pop_scope undoes every change since the matching push_scope), declared with what the registry
// reads of it, over a flat hash table, a tree or a dense map. Defined in urd/repr/scoped.hpp, which
// urd/use/scopes.hpp includes; the map it scopes comes with the map's family.

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
