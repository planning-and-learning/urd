#pragma once

// gtl's sharded hash sets (flat, or a node per element), defined. Include it where a Store resolves to one.

#include "urd/registry/declaration.hpp"
#include "urd/repr/core/gtl_memory.hpp"
#include "urd/repr/values/parallel_hash_set_fwd.hpp"
#include "urd/traits.hpp"

#include <cstddef>
#include <gtl/phmap.hpp>

namespace urd
{

template<typename T, typename H, typename E, typename A, std::size_t N, typename M, typename C>
struct memory_of<gtl::parallel_flat_hash_set<T, H, E, A, N, M, C>>
{
    static std::size_t bytes(const gtl::parallel_flat_hash_set<T, H, E, A, N, M, C>& held) { return detail::sharded_table_bytes(held); }
};
template<typename T, typename H, typename E, typename A, std::size_t N, typename M, typename C>
struct memory_of<gtl::parallel_node_hash_set<T, H, E, A, N, M, C>>
{
    static std::size_t bytes(const gtl::parallel_node_hash_set<T, H, E, A, N, M, C>& held) { return detail::sharded_table_bytes(held); }
};

template<>
struct include<"urd/repr/values/parallel_hash_set.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<parallel_hash_set_decl, S>
{
    using type = repr::ParallelHashSet<typename S::element_type>;
};
template<typename S>
struct representation_of<parallel_node_hash_set_decl, S>
{
    using type = repr::ParallelNodeHashSet<typename S::element_type>;
};

}  // namespace urd::detail
