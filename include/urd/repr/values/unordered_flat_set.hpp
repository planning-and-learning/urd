#pragma once

// Boost's open-addressing hash set, defined. Include it where a Store resolves to one.

#include "urd/elements/prehashed.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/core/boost_memory.hpp"
#include "urd/repr/values/unordered_flat_set_fwd.hpp"
#include "urd/traits.hpp"

#include <boost/unordered/unordered_flat_set.hpp>

namespace urd
{

template<>
struct include<"urd/repr/values/unordered_flat_set.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<unordered_flat_set_decl, S>
{
    using type = boost::unordered_flat_set<typename S::element_type, prehash_hash<typename S::element_type>, prehash_equal<typename S::element_type>>;
};

}  // namespace urd::detail
