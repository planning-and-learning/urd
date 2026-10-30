#pragma once

// The pools family: rows and slots reached by handle. The row interner (rows kept in full, each once), the tree
// pools (rows as trees whose equal subtrees are stored once), and the slot vector (elements in slots that never
// move). Include it where a Store of one is held. Pulls in Boost.Unordered (the interning tables).

#include "urd/repr/pools.hpp"
#include "urd/repr/slots.hpp"
#include "urd/repr/tree_bitset.hpp"
#include "urd/repr/tree_vector.hpp"
#include "urd/traits.hpp"

namespace urd
{

template<>
struct include<"urd/use/pools.hpp">
{
};

}  // namespace urd
