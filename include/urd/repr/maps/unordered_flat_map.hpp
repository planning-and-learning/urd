#pragma once

// Boost's open-addressing hash map, defined. Include it where a Store resolves to one.

#include "urd/elements/prehashed.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/core/boost_memory.hpp"
#include "urd/repr/maps/unordered_flat_map_fwd.hpp"
#include "urd/traits.hpp"

#include <boost/unordered/unordered_flat_map.hpp>

namespace urd
{

template<>
struct include<"urd/repr/maps/unordered_flat_map.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<unordered_flat_map_decl, S>
{
    using type = boost::unordered_flat_map<typename S::key_type, typename S::mapped_type, prehash_hash<typename S::key_type>, prehash_equal<typename S::key_type>>;
};

}  // namespace urd::detail
