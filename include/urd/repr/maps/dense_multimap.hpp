#pragma once

// A multimap by key index with a growing row per key, defined. Include it where a Store resolves to one.

#include "urd/registry/declaration.hpp"
#include "urd/repr/maps/dense_groups.hpp"
#include "urd/repr/maps/grouped_multimap.hpp"
#include "urd/traits.hpp"

template<>
struct urd::include<"urd/repr/maps/dense_multimap.hpp">
{
};

namespace urd::detail
{

template<typename S>
struct representation_of<dense_multimap_decl, S>
{
    using type = repr::DenseMultiMap<typename S::key_type, typename S::mapped_type>;
};

}  // namespace urd::detail
