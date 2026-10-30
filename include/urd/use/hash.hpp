#pragma once

// The hash family: the containers on Boost.Unordered's open-addressing tables. The flat hash set and map, the
// indexed sets (an interner: elements by position, positions by hash; stable or thread-safe), the insertion-ordered
// map over one, the postings (a multimap whose values lie together per key), and the change log (its log is two
// indexed sets). Include it where a Store of one is held. Pulls in Boost.Unordered.

#include "urd/repr/change_logged.hpp"
#include "urd/repr/flat_hash.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/postings.hpp"
#include "urd/traits.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>

namespace urd
{

template<>
struct include<"urd/use/hash.hpp">
{
};

}  // namespace urd
