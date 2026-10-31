#pragma once

// A multimap by key index with a growing row per key, defined. Include it where a Store resolves to one.

#include "urd/registry/row.hpp"
#include "urd/repr/dense_groups.hpp"
#include "urd/repr/grouped_multimap.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/repr/dense_multimap.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<"dense_multimap", S>
{
    using type = repr::DenseMultiMap<typename S::key_type, typename S::mapped_type>;
};

}  // namespace urd::detail
