#pragma once

// gtl's sharded hash tables (flat, or a node per element), defined. Include it where a Store resolves to one.

#include "urd/registry/row.hpp"
#include "urd/repr/parallel.hpp"
#include "urd/traits.hpp"

#include <gtl/phmap.hpp>

namespace urd
{

template<>
struct include<"urd/repr/parallel_hash.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<"parallel_hash_set", S>
{
    using type = repr::ParallelHashSet<typename S::element_type>;
};
template<typename S>
struct representation_of<"parallel_node_hash_set", S>
{
    using type = repr::ParallelNodeHashSet<typename S::element_type>;
};
template<typename S>
struct representation_of<"parallel_hash_map", S>
{
    using type = repr::ParallelHashMap<typename S::key_type, typename S::mapped_type>;
};
template<typename S>
struct representation_of<"parallel_node_hash_map", S>
{
    using type = repr::ParallelNodeHashMap<typename S::key_type, typename S::mapped_type>;
};

}  // namespace urd::detail
