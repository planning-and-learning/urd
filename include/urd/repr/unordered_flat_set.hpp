#pragma once

// Boost's open-addressing hash set, defined. Include it where a Store resolves to one.

#include "urd/registry/declaration.hpp"
#include "urd/repr/flat_hash.hpp"
#include "urd/traits.hpp"

#include <boost/unordered/unordered_flat_set.hpp>

namespace urd
{

template<>
struct include<"urd/repr/unordered_flat_set.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<unordered_flat_set_decl, S>
{
    using type = boost::unordered_flat_set<typename S::element_type, urd::Hash<typename S::element_type>, urd::EqualTo<typename S::element_type>>;
};

}  // namespace urd::detail
