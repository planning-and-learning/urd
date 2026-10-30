#pragma once

// The sequences family: elements by position. std::vector, the segmented vector whose elements never move, and the
// representations of nested Stores (rows flattened into one array, and rows kept for reuse). Include it where a Store
// of one is held. Pulls in nothing beyond the standard library.

#include "urd/repr/rows.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/use/sequences.hpp">
{
};

}  // namespace urd
