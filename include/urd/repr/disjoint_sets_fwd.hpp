#pragma once

// A partition of indices into disjoint classes (union-find), declared with what the registry reads of it. Defined in
// urd/repr/disjoint_sets.hpp, which urd/use/dense.hpp includes.

#include "urd/elements.hpp"
#include "urd/traits.hpp"

namespace urd::repr
{

/// A partition of the indices 0..n-1 into disjoint classes, each named by one of its elements. The registry names it
/// for any key type, so it is an unconstrained template; its container_traits declare properties only for
/// indexable keys.
template<typename K>
class DisjointSets;

}  // namespace urd::repr

namespace urd
{

/// Indices in classes; clear keeps the storage.
template<indexable K>
struct container_traits<repr::DisjointSets<K>> : properties_of<partition, dense, shrinkable, retains>
{
};

template<typename K>
inline constexpr auto defined_in<repr::DisjointSets<K>> = detail::fixed_string("urd/use/dense.hpp");

}  // namespace urd
