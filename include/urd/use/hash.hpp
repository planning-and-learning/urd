#pragma once

// The hash family: the containers on Boost.Unordered's open-addressing tables. For now the postings (a multimap whose
// values lie together per key in a flat hash table); the flat hash tables, the indexed sets, the insertion-ordered
// map over them and the change log join when their headers split. Include it where a Store of one is held.

#include "urd/repr/postings.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/use/hash.hpp">
{
};

}  // namespace urd
