#pragma once

// gtl's sharded hash maps (flat, or a node per element), defined. Include it where a Store resolves to one.

#include "urd/registry/declaration.hpp"
#include "urd/repr/maps/parallel_hash_map_fwd.hpp"
#include "urd/traits.hpp"

#include <gtl/phmap.hpp>

namespace urd
{

template<>
struct include<"urd/repr/maps/parallel_hash_map.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<parallel_hash_map_decl, S>
{
    using type = repr::ParallelHashMap<typename S::key_type, typename S::mapped_type>;
};
template<typename S>
struct representation_of<parallel_node_hash_map_decl, S>
{
    using type = repr::ParallelNodeHashMap<typename S::key_type, typename S::mapped_type>;
};

}  // namespace urd::detail
