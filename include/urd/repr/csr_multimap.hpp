#pragma once

// A multimap by key index in compressed sparse rows, defined. Include it where a Store resolves to one.

#include "urd/registry/row.hpp"
#include "urd/repr/grouped_multimap.hpp"
#include "urd/repr/index_groups.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/repr/csr_multimap.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<"csr_multimap", S>
{
    using type = repr::CsrMultiMap<typename S::key_type, typename S::mapped_type>;
};

}  // namespace urd::detail
