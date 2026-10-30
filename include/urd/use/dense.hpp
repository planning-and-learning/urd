#pragma once

// The dense family: containers for keys that are indices, stored by position. The bitset and sparse sets, the
// slot-per-index maps (with generations, stable slots, thread-safe slots), the insertion-ordered map over a sparse
// set, the partition, and the multimaps by key index (compressed sparse rows, their bidirectional form, growing
// rows). Include it where a Store of one is held. Pulls in nothing beyond the standard library.

#include "urd/repr/dense.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/grouped.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/use/dense.hpp">
{
};

}  // namespace urd
