#pragma once

// Boost's open-addressing hash map, defined. Include it where a Store resolves to one.

#include "urd/registry/declaration.hpp"
#include "urd/repr/flat_hash.hpp"
#include "urd/traits.hpp"

#include <boost/unordered/unordered_flat_map.hpp>

namespace urd
{

template<>
struct include<"urd/repr/unordered_flat_map.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<unordered_flat_map_decl, S>
{
    using type = boost::unordered_flat_map<typename S::key_type, typename S::mapped_type, urd::Hash<typename S::key_type>, urd::EqualTo<typename S::key_type>>;
};

}  // namespace urd::detail
